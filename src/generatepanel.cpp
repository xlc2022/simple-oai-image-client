#include "generatepanel.h"

#include "imageprovider.h"
#include "imageview.h"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QShortcut>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTimer>
#include <QVBoxLayout>

GeneratePanel::GeneratePanel(QWidget *parent) : QWidget(parent)
{
    m_prompt = new QPlainTextEdit(this);
    m_prompt->setPlaceholderText(
        tr("Describe the image you want, e.g. \"A watercolor fox reading a book under a lamp\""));
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

    m_generateButton = new QPushButton(tr("Generate"), this);
    m_generateButton->setDefault(true);
    m_generateButton->setToolTip(tr("Generate (Ctrl+Enter)"));

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
    options->addWidget(m_generateButton);

    m_status = new QLabel(this);
    m_status->setWordWrap(true);
    m_status->setTextInteractionFlags(Qt::TextSelectableByMouse);

    m_view = new ImageView(this);
    m_view->setPlaceholderText(tr("Your image will appear here."));

    m_thumbnails = new QListWidget(this);
    m_thumbnails->setViewMode(QListView::IconMode);
    m_thumbnails->setFlow(QListView::LeftToRight);
    m_thumbnails->setWrapping(false);
    m_thumbnails->setIconSize(QSize(96, 96));
    m_thumbnails->setFixedHeight(96 + 24);
    m_thumbnails->setMovement(QListView::Static);
    m_thumbnails->hide();

    m_saveButton = new QPushButton(tr("Save…"), this);
    m_copyButton = new QPushButton(tr("Copy"), this);
    m_saveButton->setEnabled(false);
    m_copyButton->setEnabled(false);

    auto *actions = new QHBoxLayout;
    actions->addStretch();
    actions->addWidget(m_copyButton);
    actions->addWidget(m_saveButton);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_prompt);
    layout->addLayout(options);
    layout->addWidget(m_status);
    layout->addWidget(m_view, 1);
    layout->addWidget(m_thumbnails);
    layout->addLayout(actions);

    m_ticker = new QTimer(this);
    m_ticker->setInterval(1000);

    connect(m_generateButton, &QPushButton::clicked, this, &GeneratePanel::startOrCancel);
    for (const auto &key : {QKeySequence(Qt::CTRL | Qt::Key_Return), QKeySequence(Qt::CTRL | Qt::Key_Enter)}) {
        auto *shortcut = new QShortcut(key, this);
        shortcut->setContext(Qt::WidgetWithChildrenShortcut);
        connect(shortcut, &QShortcut::activated, this, [this] {
            if (!m_provider || !m_provider->isBusy())
                startOrCancel();
        });
    }
    connect(m_thumbnails, &QListWidget::currentRowChanged, this, &GeneratePanel::selectResult);
    connect(m_saveButton, &QPushButton::clicked, this, &GeneratePanel::saveCurrent);
    connect(m_copyButton, &QPushButton::clicked, this, &GeneratePanel::copyCurrent);
    connect(m_ticker, &QTimer::timeout, this, &GeneratePanel::updateElapsed);
}

void GeneratePanel::setProvider(ImageProvider *provider)
{
    if (m_provider)
        m_provider->disconnect(this);
    m_provider = provider;
    setBusy(false);
    if (!provider)
        return;
    connect(provider, &ImageProvider::imagesReady, this, &GeneratePanel::showResults);
    connect(provider, &ImageProvider::requestFailed, this, &GeneratePanel::showError);
}

void GeneratePanel::startOrCancel()
{
    if (m_provider && m_provider->isBusy()) {
        m_provider->cancel();
        setBusy(false);
        m_status->setText(tr("Cancelled."));
        return;
    }

    const QString prompt = m_prompt->toPlainText().trimmed();
    if (prompt.isEmpty()) {
        m_status->setText(tr("Describe what you want to see first."));
        m_prompt->setFocus();
        return;
    }
    if (!m_provider) {
        showError(tr("No API key is set. Add one in File → Settings."));
        return;
    }

    GenerateOptions options;
    options.prompt = prompt;
    options.size = m_size->currentData().toString();
    options.quality = m_quality->currentData().toString();
    options.count = m_count->value();

    setBusy(true);
    m_provider->generate(options);
}

void GeneratePanel::setBusy(bool busy)
{
    m_generateButton->setText(busy ? tr("Cancel") : tr("Generate"));
    m_prompt->setReadOnly(busy);
    m_size->setEnabled(!busy);
    m_quality->setEnabled(!busy);
    m_count->setEnabled(!busy);
    if (busy) {
        m_elapsed.start();
        m_status->setStyleSheet(QString());
        updateElapsed();
        m_ticker->start();
    } else {
        m_ticker->stop();
    }
}

void GeneratePanel::updateElapsed()
{
    m_status->setText(tr("Generating… %1 s").arg(m_elapsed.elapsed() / 1000));
}

void GeneratePanel::showResults(const QList<QImage> &images)
{
    const qint64 seconds = m_elapsed.isValid() ? m_elapsed.elapsed() / 1000 : 0;
    setBusy(false);
    m_results = images;

    m_thumbnails->clear();
    for (const QImage &image : images) {
        auto *item = new QListWidgetItem(m_thumbnails);
        item->setIcon(QIcon(QPixmap::fromImage(
            image.scaled(96, 96, Qt::KeepAspectRatio, Qt::SmoothTransformation))));
    }
    m_thumbnails->setVisible(images.size() > 1);
    m_thumbnails->setCurrentRow(0);
    selectResult(0);

    m_status->setText(images.size() == 1 ? tr("Done in %1 s.").arg(seconds)
                                         : tr("%1 images in %2 s.").arg(images.size()).arg(seconds));
}

void GeneratePanel::showError(const QString &message)
{
    setBusy(false);
    m_status->setStyleSheet(QStringLiteral("color: #d64545;"));
    m_status->setText(QStringLiteral("✘ ") + message);
}

void GeneratePanel::selectResult(int index)
{
    const bool valid = index >= 0 && index < m_results.size();
    m_view->setImage(valid ? m_results.at(index) : QImage());
    m_saveButton->setEnabled(valid);
    m_copyButton->setEnabled(valid);
}

void GeneratePanel::saveCurrent()
{
    const QImage image = m_view->image();
    if (image.isNull())
        return;

    const QString dir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    const QString suggested = QDir(dir).filePath(
        QStringLiteral("image-%1.png").arg(QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss")));
    const QString path = QFileDialog::getSaveFileName(this, tr("Save Image"), suggested,
                                                      tr("PNG image (*.png);;JPEG image (*.jpg)"));
    if (path.isEmpty())
        return;
    if (!image.save(path))
        QMessageBox::warning(this, tr("Save Image"), tr("Could not save to %1.").arg(path));
}

void GeneratePanel::copyCurrent()
{
    if (!m_view->image().isNull())
        QApplication::clipboard()->setImage(m_view->image());
}
