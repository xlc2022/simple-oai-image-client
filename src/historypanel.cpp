#include "historypanel.h"

#include "imageactions.h"
#include "imageview.h"

#include <QDesktopServices>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QSplitter>
#include <QStackedLayout>
#include <QUrl>
#include <QVBoxLayout>

namespace {
constexpr int kEntryRole = Qt::UserRole;
constexpr int kGridIcon = 128;
constexpr int kStripIcon = 72;

// Thumbnails keep their real colours when selected; selection is a frame.
const char *kFramedItems =
    "QListWidget::item { border: 2px solid transparent; padding: 2px; }"
    "QListWidget::item:selected { border-color: palette(highlight); background: transparent;"
    " color: palette(text); }";

QIcon untintedIcon(const QPixmap &pixmap)
{
    QIcon icon;
    icon.addPixmap(pixmap, QIcon::Normal);
    icon.addPixmap(pixmap, QIcon::Selected);
    return icon;
}
} // namespace

HistoryPanel::HistoryPanel(HistoryStore *store, QWidget *parent) : QWidget(parent), m_store(store)
{
    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(tr("Search prompts"));
    m_search->setClearButtonEnabled(true);

    m_grid = new QListWidget(this);
    m_grid->setViewMode(QListView::IconMode);
    m_grid->setResizeMode(QListView::Adjust);
    m_grid->setMovement(QListView::Static);
    m_grid->setIconSize(QSize(kGridIcon, kGridIcon));
    // Room for a two-line caption (prompt, date) under each thumbnail.
    m_grid->setGridSize(QSize(kGridIcon + 16, kGridIcon + 16 + 2 * m_grid->fontMetrics().height()));
    m_grid->setWordWrap(true);
    m_grid->setUniformItemSizes(true);
    m_grid->setStyleSheet(QString::fromLatin1(kFramedItems));

    m_empty = new QLabel(tr("Nothing here yet. Everything you generate or edit is kept here."), this);
    m_empty->setAlignment(Qt::AlignCenter);
    m_empty->setWordWrap(true);

    m_gridStack = new QStackedLayout;
    m_gridStack->addWidget(m_grid);
    m_gridStack->addWidget(m_empty);

    auto *left = new QWidget(this);
    auto *leftLayout = new QVBoxLayout(left);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->addWidget(m_search);
    leftLayout->addLayout(m_gridStack, 1);

    m_view = new ImageView(this);
    m_view->setPlaceholderText(tr("Select an entry to see it here."));

    m_images = new QListWidget(this);
    m_images->setViewMode(QListView::IconMode);
    m_images->setFlow(QListView::LeftToRight);
    m_images->setWrapping(false);
    m_images->setMovement(QListView::Static);
    m_images->setIconSize(QSize(kStripIcon, kStripIcon));
    m_images->setFixedHeight(kStripIcon + 40);
    m_images->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_images->setStyleSheet(QString::fromLatin1(kFramedItems));

    m_prompt = new QLabel(this);
    m_prompt->setWordWrap(true);
    m_prompt->setTextInteractionFlags(Qt::TextSelectableByMouse);
    QFont promptFont = m_prompt->font();
    promptFont.setPointSizeF(promptFont.pointSizeF() * 1.1);
    m_prompt->setFont(promptFont);

    m_meta = new QLabel(this);
    m_meta->setWordWrap(true);
    m_meta->setForegroundRole(QPalette::PlaceholderText);

    m_reuseButton = new QPushButton(tr("Reuse Prompt"), this);
    m_reuseButton->setToolTip(tr("Fill in this prompt and its options on the tab it came from"));
    m_editButton = new QPushButton(tr("Edit This…"), this);
    m_editButton->setToolTip(tr("Open the shown image on the Edit tab"));
    m_copyButton = new QPushButton(tr("Copy"), this);
    m_saveButton = new QPushButton(tr("Save…"), this);
    m_deleteButton = new QPushButton(tr("Delete"), this);
    auto *openFolder = new QPushButton(tr("Open Folder"), this);
    openFolder->setToolTip(tr("Show this entry's files"));

    m_deleteButton->setShortcut(QKeySequence::Delete);
    m_deleteButton->setToolTip(tr("Delete this entry (Del)"));

    // Delete stands apart so it's never one slip away from Copy or Save.
    auto *actions = new QHBoxLayout;
    actions->addWidget(m_reuseButton);
    actions->addWidget(m_editButton);
    actions->addStretch();
    actions->addWidget(m_deleteButton);
    actions->addSpacing(24);
    actions->addWidget(openFolder);
    actions->addWidget(m_copyButton);
    actions->addWidget(m_saveButton);

    m_details = new QWidget(this);
    auto *detailsLayout = new QVBoxLayout(m_details);
    detailsLayout->setContentsMargins(0, 0, 0, 0);
    detailsLayout->addWidget(m_view, 1);
    detailsLayout->addWidget(m_images);
    detailsLayout->addWidget(m_prompt);
    detailsLayout->addWidget(m_meta);
    detailsLayout->addLayout(actions);

    auto *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(left);
    splitter->addWidget(m_details);
    splitter->setChildrenCollapsible(false);
    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 3);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(splitter);

    connect(m_search, &QLineEdit::textChanged, this, &HistoryPanel::applyFilter);
    connect(m_grid, &QListWidget::currentRowChanged, this, &HistoryPanel::showEntry);
    connect(m_grid, &QListWidget::itemDoubleClicked, this, [this] {
        if (const HistoryEntry *entry = currentEntry())
            emit reuseRequested(*entry);
    });
    connect(m_images, &QListWidget::currentRowChanged, this, &HistoryPanel::showImage);
    connect(m_reuseButton, &QPushButton::clicked, this, [this] {
        if (const HistoryEntry *entry = currentEntry())
            emit reuseRequested(*entry);
    });
    connect(m_editButton, &QPushButton::clicked, this, [this] {
        if (const QImage image = currentImage(); !image.isNull())
            emit editRequested(image);
    });
    connect(m_copyButton, &QPushButton::clicked, this,
            [this] { ImageActions::copyToClipboard(currentImage()); });
    connect(m_saveButton, &QPushButton::clicked, this,
            [this] { ImageActions::saveWithDialog(this, currentImage()); });
    connect(m_deleteButton, &QPushButton::clicked, this, &HistoryPanel::deleteCurrent);
    connect(openFolder, &QPushButton::clicked, this, [this] {
        if (const HistoryEntry *entry = currentEntry())
            QDesktopServices::openUrl(QUrl::fromLocalFile(entry->dir));
    });

    reload();
}

void HistoryPanel::reload(const QString &selectId)
{
    const HistoryEntry *current = currentEntry();
    const QString keep = !selectId.isEmpty() ? selectId : current ? current->id : QString();

    m_entries = m_store->entries();
    m_grid->clear();
    for (int i = 0; i < m_entries.size(); ++i) {
        QListWidgetItem *item = createItem(i, i);
        if (m_entries[i].id == keep)
            m_grid->setCurrentItem(item);
    }
    if (!m_grid->currentItem() && m_grid->count() > 0)
        m_grid->setCurrentRow(0);
    applyFilter();
}

void HistoryPanel::addEntry(const HistoryEntry &entry)
{
    // Appended to m_entries so existing items' indices stay valid; shown first in the grid.
    m_entries.append(entry);
    QListWidgetItem *item = createItem(int(m_entries.size()) - 1, 0);
    if (!m_grid->currentItem())
        m_grid->setCurrentItem(item);
    applyFilter();
}

QListWidgetItem *HistoryPanel::createItem(int entryIndex, int row)
{
    const HistoryEntry &entry = m_entries[entryIndex];
    auto *item = new QListWidgetItem(untintedIcon(QPixmap(entry.thumbnailFile)), QString());
    const QFontMetrics fm = m_grid->fontMetrics();
    item->setText(fm.elidedText(entry.prompt, Qt::ElideRight, kGridIcon) + QLatin1Char('\n')
                  + QLocale().toString(entry.created.date(), QLocale::ShortFormat));
    item->setTextAlignment(Qt::AlignHCenter | Qt::AlignTop);
    item->setData(kEntryRole, entryIndex);
    item->setToolTip(entry.prompt);
    m_grid->insertItem(row, item);
    return item;
}

void HistoryPanel::applyFilter()
{
    const QString needle = m_search->text().trimmed();
    int visible = 0;
    for (int row = 0; row < m_grid->count(); ++row) {
        QListWidgetItem *item = m_grid->item(row);
        const HistoryEntry &entry = m_entries[item->data(kEntryRole).toInt()];
        const bool match = needle.isEmpty() || entry.prompt.contains(needle, Qt::CaseInsensitive);
        item->setHidden(!match);
        visible += match;
    }
    m_empty->setText(m_entries.isEmpty()
                         ? tr("Nothing here yet. Everything you generate or edit is kept here.")
                         : tr("No prompts match \"%1\".").arg(needle));
    m_gridStack->setCurrentWidget(visible > 0 ? static_cast<QWidget *>(m_grid) : m_empty);
    // An empty store already says so on the left; don't repeat it on the right.
    m_details->setVisible(!m_entries.isEmpty());

    // Never leave the details showing an entry the filter just hid.
    if (QListWidgetItem *cur = m_grid->currentItem(); !cur || cur->isHidden()) {
        int first = -1;
        for (int row = 0; row < m_grid->count() && first < 0; ++row)
            if (!m_grid->item(row)->isHidden())
                first = row;
        m_grid->setCurrentRow(first); // -1 clears
    }
    showEntry(m_grid->currentRow());
}

const HistoryEntry *HistoryPanel::currentEntry() const
{
    const QListWidgetItem *item = m_grid->currentItem();
    if (!item || item->isHidden())
        return nullptr;
    const int index = item->data(kEntryRole).toInt();
    return index >= 0 && index < m_entries.size() ? &m_entries[index] : nullptr;
}

QImage HistoryPanel::currentImage() const
{
    return m_view->image();
}

void HistoryPanel::showEntry(int)
{
    const HistoryEntry *entry = currentEntry();
    m_details->setEnabled(entry != nullptr);
    m_images->clear();
    if (!entry) {
        m_view->setImage(QImage());
        m_prompt->clear();
        m_meta->clear();
        m_images->hide();
        return;
    }

    for (int i = 0; i < entry->resultFiles.size(); ++i) {
        const QPixmap thumb = QPixmap(entry->resultFiles[i])
                                  .scaled(kStripIcon, kStripIcon, Qt::KeepAspectRatio,
                                          Qt::SmoothTransformation);
        auto *item = new QListWidgetItem(untintedIcon(thumb), QString(), m_images);
        item->setToolTip(tr("Result %1 of %2").arg(i + 1).arg(entry->resultFiles.size()));
    }
    if (!entry->sourceFile.isEmpty()) {
        const QPixmap thumb = QPixmap(entry->sourceFile)
                                  .scaled(kStripIcon, kStripIcon, Qt::KeepAspectRatio,
                                          Qt::SmoothTransformation);
        auto *item = new QListWidgetItem(untintedIcon(thumb), tr("Original"), m_images);
        item->setToolTip(tr("The photo before editing"));
    }
    m_images->setVisible(m_images->count() > 1);
    m_images->setCurrentRow(0);
    showImage(0);

    m_prompt->setText(entry->prompt);
    QStringList meta{
        QLocale().toString(entry->created, QLocale::ShortFormat),
        entry->isEdit ? tr("Edit") : tr("Generate"),
        entry->provider + QStringLiteral(" · ") + entry->model,
    };
    if (entry->size != QLatin1String("auto"))
        meta << entry->size;
    if (entry->quality != QLatin1String("auto") && !entry->quality.isEmpty())
        meta << tr("%1 quality").arg(entry->quality);
    m_meta->setText(meta.join(QStringLiteral("  ·  ")));
}

void HistoryPanel::showImage(int index)
{
    const HistoryEntry *entry = currentEntry();
    if (!entry || index < 0) {
        m_view->setImage(QImage());
        return;
    }
    const QString file = index < entry->resultFiles.size() ? entry->resultFiles[index]
                                                           : entry->sourceFile;
    m_view->setImage(QImage(file));
}

void HistoryPanel::deleteCurrent()
{
    const HistoryEntry *entry = currentEntry();
    if (!entry)
        return;
    const auto answer = QMessageBox::question(
        this, tr("Delete from History"),
        tr("Delete this entry and its %n image(s)? This can't be undone.", nullptr,
           int(entry->resultFiles.size())));
    if (answer != QMessageBox::Yes)
        return;

    // Select the neighbour so the user can keep going.
    const int row = m_grid->currentRow();
    QString next;
    for (int r : {row + 1, row - 1}) {
        if (QListWidgetItem *item = m_grid->item(r); item && !item->isHidden())
            next = m_entries[item->data(kEntryRole).toInt()].id;
        if (!next.isEmpty())
            break;
    }
    if (!m_store->remove(entry->id))
        QMessageBox::warning(this, tr("Delete from History"),
                             tr("Could not delete %1.").arg(entry->dir));
    reload(next);
}
