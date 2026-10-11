#pragma once

#include <QDateTime>
#include <QImage>
#include <QList>
#include <QString>
#include <QStringList>

// One Generate or Edit request and what it produced.
struct HistoryEntry {
    QString id;
    QDateTime created;
    bool isEdit = false;
    QString prompt;
    QString provider;
    QString model;
    QString size;
    QString quality;

    // Absolute paths, filled in by HistoryStore.
    QString dir;
    QStringList resultFiles;
    QString sourceFile; // edits only
    QString thumbnailFile;
};

// Keeps past results on disk, one folder per entry:
//   <root>/<id>/entry.json, result-1.png …, source.png (edits), thumb.png
// Folder-per-entry means a damaged entry never affects the others.
class HistoryStore
{
public:
    // Defaults to the app's data directory.
    explicit HistoryStore(QString root = QString());

    QString root() const { return m_root; }

    // Writes the images and metadata; returns the stored entry (empty id on failure).
    HistoryEntry add(HistoryEntry entry, const QList<QImage> &results,
                     const QImage &source = QImage());
    // All readable entries, newest first.
    QList<HistoryEntry> entries() const;
    bool remove(const QString &id);

    static constexpr int kThumbnailSize = 192;

private:
    HistoryEntry load(const QString &dir) const;

    QString m_root;
};
