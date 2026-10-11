#include "imagepanel.h"

#include "imageprovider.h"
#include "imageview.h"
#include "maskcanvas.h"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QFileInfo>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QShortcut>
#include <QSlider>
#include <QSplitter>
#include <QImageReader>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTimer>
#include <QVBoxLayout>

ImagePanel::ImagePanel(Mode mode, QWidget *parent) : QWidget(parent), m_mode(mode)
{
    const bool edit = mode == Mode::Edit;
    const QString runLabel = edit ? tr("Edit") : tr("Generate");

    m_prompt = new QPlainTextEdit(this);
    m_prompt->setPlaceholderText(
        edit ? tr("Describe the change, e.g. \"Replace the sky with a sunset\" or \"Make it look like a pencil sketch\"")
             : tr("Describe the image you want, e.g. \"A watercolor fox reading a book under a lamp\""));
    m_prompt->setTabChangesFocus(true);
    m_prompt->setFixedHeight(m_prompt->fontMetrics().lineSpacing() * 4 + 12);

    m_size = new QComboBox(this);
    m_size->addItem(tr("Auto"), QStringLiteral("auto"));
    m_size->addItem(tr("Square (1024×1024)"), QStringLiteral("1024x1024"));
    m_size->addItem(tr("Landscape (1536×1024)"), QStringLiteral("1536x1024"));
    m_size->addItem(tr("Portrait (1024×1536)"), QStringLiteral("1024x1536"));

    m_quality = new QComboBox(this);
    m_quality->addItem(tr("Auto"), QStringLiteral("auto"));
    m_quality->addItem(tr("Low"), QStringLiteral("low"));
    m_quality->addItem(tr("Medium"), QStringLiteral("medium"));
    m_quality->addItem(tr("High"), QStringLiteral("high"));

    m_count = new QSpinBox(this);
    m_count->setRange(1, 4);

    m_runButton = new QPushButton(runLabel, this);
    m_runButton->setToolTip(tr("%1 (Ctrl+Enter). Press Esc to cancel.").arg(runLabel));
    // Keep the width stable when the label swaps to "Cancel".
    const QFontMetrics fm = m_runButton->fontMetrics();
    m_runButton->setMinimumWidth(
        qMax(fm.horizontalAdvance(runLabel), fm.horizontalAdvance(tr("Cancel"))) + 32);

    auto *options = new QHBoxLayout;
    options->addWidget(new QLabel(tr("Size:"), this));
    options->addWidget(m_size);
    options->addSpacing(12);
    options->addWidget(new QLabel(tr("Quality:"), this));
    options->addWidget(m_quality);
    options->addSpacing(12);
    options->addWidget(new QLabel(tr("Images:"), this));
    options->addWidget(m_count);
    options->addStretch();
    options->addWidget(m_runButton);

    m_status = new QLabel(this);
    m_status->setWordWrap(true);
    m_status->setTextInteractionFlags(Qt::TextSelectableByMouse);
    // Reserve a line so the preview doesn't jump when the first status appears.
    m_status->setMinimumHeight(m_status->fontMetrics().height());

    m_progress = new QProgressBar(this);
    m_progress->setRange(0, 0); // indeterminate
    m_progress->setTextVisible(false);
    m_progress->setMaximumWidth(160);
    m_progress->hide();

    auto *statusRow = new QHBoxLayout;
    statusRow->addWidget(m_progress);
    statusRow->addWidget(m_status, 1);

    m_view = new ImageView(this);
    m_view->setPlaceholderText(edit ? tr("The edited image will appear here.")
                                    : tr("Your image will appear here."));

    m_thumbnails = new QListWidget(this);
    m_thumbnails->setViewMode(QListView::IconMode);
    m_thumbnails->setFlow(QListView::LeftToRight);
    m_thumbnails->setWrapping(false);
    m_thumbnails->setIconSize(QSize(96, 96));
    m_thumbnails->setFixedHeight(96 + 40);
    m_thumbnails->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_thumbnails->setMovement(QListView::Static);
    // Show selection as a frame; the default highlight tints the icon and misrepresents colours.
    m_thumbnails->setStyleSheet(QStringLiteral(
        "QListWidget::item { border: 2px solid transparent; padding: 2px; }"
        "QListWidget::item:selected { border-color: palette(highlight); background: transparent; }"));
    m_thumbnails->hide();

    m_saveButton = new QPushButton(tr("Save…"), this);
    m_copyButton = new QPushButton(tr("Copy"), this);
    m_useButton = new QPushButton(edit ? tr("Use as Source") : tr("Edit This…"), this);
    m_useButton->setToolTip(edit ? tr("Keep editing from this result")
                                 : tr("Open this image on the Edit tab"));
    m_saveButton->setEnabled(false);
    m_copyButton->setEnabled(false);
    m_useButton->setEnabled(false);

    auto *actions = new QHBoxLayout;
    actions->addStretch();
    actions->addWidget(m_useButton);
    actions->addWidget(m_copyButton);
    actions->addWidget(m_saveButton);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_prompt);
    layout->addLayout(options);
    layout->addLayout(statusRow);
    if (edit) {
        // Source on the left, result on the right.
        auto *resultArea = new QWidget(this);
        auto *resultLayout = new QVBoxLayout(resultArea);
        resultLayout->setContentsMargins(0, 0, 0, 0);
        resultLayout->addWidget(m_view, 1);
        resultLayout->addWidget(m_thumbnails);
        resultLayout->addLayout(actions);

        auto *splitter = new QSplitter(Qt::Horizontal, this);
        splitter->addWidget(createSourceArea());
        splitter->addWidget(resultArea);
        splitter->setChildrenCollapsible(false);
        layout->addWidget(splitter, 1);
    } else {
        layout->addWidget(m_view, 1);
        layout->addWidget(m_thumbnails);
        layout->addLayout(actions);
    }

    m_ticker = new QTimer(this);
    m_ticker->setInterval(1000);

    connect(m_runButton, &QPushButton::clicked, this, &ImagePanel::startOrCancel);
    for (const auto &key : {QKeySequence(Qt::CTRL | Qt::Key_Return), QKeySequence(Qt::CTRL | Qt::Key_Enter)}) {
        auto *shortcut = new QShortcut(key, this);
        shortcut->setContext(Qt::WidgetWithChildrenShortcut);
        connect(shortcut, &QShortcut::activated, this, [this] {
            if (!m_provider || !m_provider->isBusy())
                startOrCancel();
        });
    }
    auto *escape = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    escape->setContext(Qt::WidgetWithChildrenShortcut);
    connect(escape, &QShortcut::activated, this, [this] {
        if (m_provider && m_provider->isBusy())
            startOrCancel();
    });
    connect(m_thumbnails, &QListWidget::currentRowChanged, this, &ImagePanel::selectResult);
    connect(m_saveButton, &QPushButton::clicked, this, &ImagePanel::saveCurrent);
    connect(m_copyButton, &QPushButton::clicked, this, &ImagePanel::copyCurrent);
    connect(m_useButton, &QPushButton::clicked, this, &ImagePanel::useCurrent);
    connect(m_ticker, &QTimer::timeout, this, &ImagePanel::updateElapsed);
}

QWidget *ImagePanel::createSourceArea()
{
    auto *area = new QWidget(this);

    m_canvas = new MaskCanvas(area);
    auto *openButton = new QPushButton(tr("Open…"), area);
    auto *pasteButton = new QPushButton(tr("Paste"), area);

    m_brushSize = new QSlider(Qt::Horizontal, area);
    m_brushSize->setRange(8, 160);
    m_brushSize->setValue(40);
    m_brushSize->setMaximumWidth(140);
    m_brushSize->setToolTip(tr("Brush size"));
    m_canvas->setBrushSize(m_brushSize->value());

    m_clearMaskButton = new QPushButton(tr("Clear Mask"), area);
    m_maskHint = new QLabel(area);
    m_maskHint->setWordWrap(true);

    auto *toolbar = new QHBoxLayout;
    toolbar->addWidget(openButton);
    toolbar->addWidget(pasteButton);
    toolbar->addStretch();
    toolbar->addWidget(new QLabel(tr("Brush:"), area));
    toolbar->addWidget(m_brushSize);
    toolbar->addWidget(m_clearMaskButton);

    auto *layout = new QVBoxLayout(area);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addLayout(toolbar);
    layout->addWidget(m_canvas, 1);
    layout->addWidget(m_maskHint);

    connect(openButton, &QPushButton::clicked, this, &ImagePanel::openSource);
    connect(pasteButton, &QPushButton::clicked, this, &ImagePanel::pasteSource);
    connect(m_brushSize, &QSlider::valueChanged, m_canvas, &MaskCanvas::setBrushSize);
    connect(m_clearMaskButton, &QPushButton::clicked, m_canvas, &MaskCanvas::clearMask);
    connect(m_canvas, &MaskCanvas::imageDropped, this, &ImagePanel::setSourceImage);
    connect(m_canvas, &MaskCanvas::maskChanged, this, &ImagePanel::updateMaskControls);

    updateMaskControls();
    return area;
}

void ImagePanel::updateMaskControls()
{
    if (!m_canvas)
        return;
    const bool supported = !m_provider || m_provider->supportsMask();
    const bool hasImage = !m_canvas->image().isNull();
    m_canvas->setMaskingEnabled(supported);
    m_brushSize->setEnabled(supported && hasImage);
    m_clearMaskButton->setEnabled(m_canvas->hasMask());

    if (!supported)
        m_maskHint->setText(tr("%1 can't use a mask; the model decides what to change.")
                                .arg(m_provider->displayName()));
    else if (m_canvas->hasMask())
        m_maskHint->setText(tr("Only the painted area will be changed."));
    else if (hasImage)
        m_maskHint->setText(tr("Optional: paint over the area to change. Without a mask the "
                               "model may change anything."));
    else
        m_maskHint->clear();
}

void ImagePanel::setSourceImage(const QImage &image)
{
    if (!m_canvas || image.isNull())
        return;
    m_canvas->setImage(image);
    updateMaskControls();
}

void ImagePanel::openSource()
{
    QStringList patterns;
    for (const QByteArray &format : QImageReader::supportedImageFormats())
        patterns << QStringLiteral("*.") + QString::fromLatin1(format);
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Open Image"),
        QStandardPaths::writableLocation(QStandardPaths::PicturesLocation),
        tr("Images (%1)").arg(patterns.join(QLatin1Char(' '))));
    if (path.isEmpty())
        return;

    QImageReader reader(path);
    reader.setAutoTransform(true); // respect EXIF rotation from phone photos
    const QImage image = reader.read();
    if (image.isNull()) {
        setStatus(tr("Could not open %1: %2").arg(path, reader.errorString()), true);
        return;
    }
    setSourceImage(image);
}

void ImagePanel::pasteSource()
{
    const QImage image = QApplication::clipboard()->image();
    if (image.isNull()) {
        setStatus(tr("The clipboard doesn't contain an image."));
        return;
    }
    setSourceImage(image);
}

void ImagePanel::useCurrent()
{
    const QImage image = m_view->image();
    if (image.isNull())
        return;
    if (m_mode == Mode::Edit)
        setSourceImage(image);
    else
        emit editRequested(image);
}

void ImagePanel::setProvider(ImageProvider *provider)
{
    if (m_provider)
        m_provider->disconnect(this);
    m_provider = provider;
    setBusy(false);

    const bool quality = !provider || provider->supportsQuality();
    m_quality->setEnabled(quality);
    m_quality->setToolTip(quality ? QString() : tr("Not supported by %1").arg(provider->displayName()));

    updateMaskControls();

    if (!provider)
        return;
    connect(provider, &ImageProvider::imagesReady, this, &ImagePanel::showResults);
    connect(provider, &ImageProvider::requestFailed, this, &ImagePanel::showError);
}

void ImagePanel::startOrCancel()
{
    if (m_provider && m_provider->isBusy()) {
        m_provider->cancel();
        setBusy(false);
        setStatus(tr("Cancelled."));
        return;
    }

    const QString prompt = m_prompt->toPlainText().trimmed();
    if (prompt.isEmpty()) {
        setStatus(tr("Describe what you want to see first."));
        m_prompt->setFocus();
        return;
    }
    if (m_canvas && m_canvas->image().isNull()) {
        setStatus(tr("Open or drop the photo you want to edit first."));
        return;
    }
    if (!m_provider) {
        showError(tr("No API key is set. Add one in File → Settings."));
        return;
    }

    EditOptions options;
    options.prompt = prompt;
    options.size = m_size->currentData().toString();
    options.quality = m_quality->currentData().toString();
    options.count = m_count->value();

    setBusy(true);
    if (m_mode == Mode::Edit) {
        options.image = m_canvas->image();
        if (m_provider->supportsMask())
            options.mask = m_canvas->mask();
        m_provider->edit(options);
    } else {
        m_provider->generate(options);
    }
}

void ImagePanel::setBusy(bool busy)
{
    m_runButton->setText(busy ? tr("Cancel")
                              : m_mode == Mode::Edit ? tr("Edit") : tr("Generate"));
    m_prompt->setReadOnly(busy);
    m_size->setEnabled(!busy);
    m_quality->setEnabled(!busy && (!m_provider || m_provider->supportsQuality()));
    m_count->setEnabled(!busy);
    m_progress->setVisible(busy);
    if (busy) {
        m_elapsed.start();
        updateElapsed();
        m_ticker->start();
    } else {
        m_ticker->stop();
    }
}

void ImagePanel::updateElapsed()
{
    const qint64 seconds = m_elapsed.elapsed() / 1000;
    setStatus(m_mode == Mode::Edit ? tr("Editing… %1 s").arg(seconds)
                                   : tr("Generating… %1 s").arg(seconds));
}

void ImagePanel::showResults(const QList<QImage> &images)
{
    // Only meaningful if a request was actually timed.
    const qint64 seconds = m_ticker->isActive() ? m_elapsed.elapsed() / 1000 : 0;
    setBusy(false);
    m_results = images;

    m_thumbnails->clear();
    for (int i = 0; i < images.size(); ++i) {
        const QPixmap thumb = QPixmap::fromImage(
            images[i].scaled(96, 96, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        QIcon icon;
        icon.addPixmap(thumb, QIcon::Normal);
        icon.addPixmap(thumb, QIcon::Selected);
        auto *item = new QListWidgetItem(icon, QString(), m_thumbnails);
        item->setToolTip(tr("Image %1 of %2").arg(i + 1).arg(images.size()));
    }
    m_thumbnails->setVisible(images.size() > 1);
    m_thumbnails->setCurrentRow(0);
    selectResult(0);

    setStatus(images.size() == 1 ? tr("Done in %1 s.").arg(seconds)
                                 : tr("%1 images in %2 s.").arg(images.size()).arg(seconds));
}

void ImagePanel::showError(const QString &message)
{
    setBusy(false);
    setStatus(QStringLiteral("✘ ") + message, true);
}

void ImagePanel::setStatus(const QString &text, bool error)
{
    if (error) {
        // A red that keeps its contrast on both light and dark palettes.
        const bool darkTheme = palette().color(QPalette::Window).lightness() < 128;
        m_status->setStyleSheet(darkTheme ? QStringLiteral("color: #ff7b7b;")
                                          : QStringLiteral("color: #c62828;"));
    } else {
        m_status->setStyleSheet(QString());
    }
    m_status->setText(text);
}

void ImagePanel::selectResult(int index)
{
    const bool valid = index >= 0 && index < m_results.size();
    m_view->setImage(valid ? m_results.at(index) : QImage());
    m_saveButton->setEnabled(valid);
    m_copyButton->setEnabled(valid);
    m_useButton->setEnabled(valid);
}

void ImagePanel::saveCurrent()
{
    const QImage image = m_view->image();
    if (image.isNull())
        return;

    const QString dir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    const QString suggested = QDir(dir).filePath(
        QStringLiteral("image-%1.png").arg(QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss")));
    const QString pngFilter = tr("PNG image (*.png)");
    const QString jpegFilter = tr("JPEG image (*.jpg *.jpeg)");
    QString selectedFilter = pngFilter;
    QString path = QFileDialog::getSaveFileName(this, tr("Save Image"), suggested,
                                                pngFilter + QStringLiteral(";;") + jpegFilter,
                                                &selectedFilter);
    if (path.isEmpty())
        return;

    // QImage::save picks the format from the suffix, so make it match the chosen filter.
    const bool wantJpeg = selectedFilter == jpegFilter;
    const QString suffix = QFileInfo(path).suffix().toLower();
    const bool isJpeg = suffix == QLatin1String("jpg") || suffix == QLatin1String("jpeg");
    if (wantJpeg != isJpeg || suffix.isEmpty()) {
        if (!suffix.isEmpty())
            path.chop(suffix.size() + 1);
        path += wantJpeg ? QStringLiteral(".jpg") : QStringLiteral(".png");
    }

    if (!image.save(path))
        QMessageBox::warning(this, tr("Save Image"), tr("Could not save to %1.").arg(path));
}

void ImagePanel::copyCurrent()
{
    if (!m_view->image().isNull())
        QApplication::clipboard()->setImage(m_view->image());
}
