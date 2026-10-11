#pragma once

#include <QList>
#include <QWidget>

#include "historystore.h"

class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QStackedLayout;
class ImageView;

// Browses past results: a grid of entries, and the selected entry's images,
// prompt and details with actions to reuse them.
class HistoryPanel : public QWidget
{
    Q_OBJECT

public:
    explicit HistoryPanel(HistoryStore *store, QWidget *parent = nullptr);

    // Re-reads the store; keeps the selection when the entry still exists.
    void reload(const QString &selectId = QString());

signals:
    void reuseRequested(const HistoryEntry &entry);
    void editRequested(const QImage &image);

private:
    void applyFilter();
    void showEntry(int row);
    void showImage(int index);
    void deleteCurrent();
    const HistoryEntry *currentEntry() const;
    QImage currentImage() const;

    HistoryStore *m_store;
    QList<HistoryEntry> m_entries;

    QLineEdit *m_search;
    QListWidget *m_grid;
    QLabel *m_empty;
    QStackedLayout *m_gridStack;
    QWidget *m_details;
    ImageView *m_view;
    QListWidget *m_images;
    QLabel *m_prompt;
    QLabel *m_meta;
    QPushButton *m_reuseButton;
    QPushButton *m_editButton;
    QPushButton *m_copyButton;
    QPushButton *m_saveButton;
    QPushButton *m_deleteButton;
};
