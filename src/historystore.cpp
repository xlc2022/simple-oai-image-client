#include "historystore.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QSaveFile>
#include <QStandardPaths>

#include <algorithm>

HistoryStore::HistoryStore(QString root) : m_root(std::move(root))
{
    if (m_root.isEmpty())
        m_root = QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
                     .filePath(QStringLiteral("history"));
}

HistoryEntry HistoryStore::add(HistoryEntry entry, const QList<QImage> &results,
                               const QImage &source)
{
    if (results.isEmpty())
        return {};

    if (!entry.created.isValid())
        entry.created = QDateTime::currentDateTime();
    // Sortable and unique enough for one person's history.
    entry.id = entry.created.toString(QStringLiteral("yyyyMMdd-HHmmss-zzz")) + QLatin1Char('-')
               + QString::number(QRandomGenerator::global()->bounded(0x10000), 16);

    QDir dir(m_root);
    if (!dir.mkpath(entry.id) || !dir.cd(entry.id))
        return {};
    entry.dir = dir.absolutePath();

    QJsonArray resultNames;
    for (int i = 0; i < results.size(); ++i) {
        const QString name = QStringLiteral("result-%1.png").arg(i + 1);
        if (!results[i].save(dir.filePath(name)))
            return {};
        resultNames.append(name);
    }
    results.first()
        .scaled(kThumbnailSize, kThumbnailSize, Qt::KeepAspectRatio, Qt::SmoothTransformation)
        .save(dir.filePath(QStringLiteral("thumb.png")));
    if (entry.isEdit && !source.isNull())
        source.save(dir.filePath(QStringLiteral("source.png")));

    const QJsonObject json{
        {"version", 1},
        {"created", entry.created.toString(Qt::ISODateWithMs)},
        {"mode", entry.isEdit ? "edit" : "generate"},
        {"prompt", entry.prompt},
        {"provider", entry.provider},
        {"model", entry.model},
        {"size", entry.size},
        {"quality", entry.quality},
        {"results", resultNames},
    };
    // Written last and atomically: a folder without entry.json is ignored.
    QSaveFile file(dir.filePath(QStringLiteral("entry.json")));
    if (!file.open(QIODevice::WriteOnly))
        return {};
    file.write(QJsonDocument(json).toJson());
    if (!file.commit())
        return {};

    return load(entry.dir);
}

HistoryEntry HistoryStore::load(const QString &path) const
{
    QFile file(QDir(path).filePath(QStringLiteral("entry.json")));
    if (!file.open(QIODevice::ReadOnly))
        return {};
    const QJsonObject json = QJsonDocument::fromJson(file.readAll()).object();
    if (json.isEmpty())
        return {};

    const QDir dir(path);
    HistoryEntry entry;
    entry.id = dir.dirName();
    entry.dir = dir.absolutePath();
    entry.created = QDateTime::fromString(json["created"].toString(), Qt::ISODateWithMs);
    entry.isEdit = json["mode"].toString() == QLatin1String("edit");
    entry.prompt = json["prompt"].toString();
    entry.provider = json["provider"].toString();
    entry.model = json["model"].toString();
    entry.size = json["size"].toString();
    entry.quality = json["quality"].toString();
    for (const QJsonValue &name : json["results"].toArray()) {
        const QString file = dir.filePath(name.toString());
        if (QFile::exists(file))
            entry.resultFiles.append(file);
    }
    if (entry.resultFiles.isEmpty())
        return {};
    if (QFile::exists(dir.filePath(QStringLiteral("source.png"))))
        entry.sourceFile = dir.filePath(QStringLiteral("source.png"));
    entry.thumbnailFile = QFile::exists(dir.filePath(QStringLiteral("thumb.png")))
                              ? dir.filePath(QStringLiteral("thumb.png"))
                              : entry.resultFiles.first();
    return entry;
}

QList<HistoryEntry> HistoryStore::entries() const
{
    QList<HistoryEntry> list;
    const QDir root(m_root);
    for (const QString &name : root.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        HistoryEntry entry = load(root.filePath(name));
        if (!entry.id.isEmpty())
            list.append(entry);
    }
    std::sort(list.begin(), list.end(), [](const HistoryEntry &a, const HistoryEntry &b) {
        return a.created > b.created;
    });
    return list;
}

bool HistoryStore::remove(const QString &id)
{
    // Guard against ids that would escape the history folder.
    if (id.isEmpty() || id.contains(QLatin1Char('/')) || id.contains(QLatin1Char('\\'))
        || id.startsWith(QLatin1Char('.')))
        return false;
    QDir dir(QDir(m_root).filePath(id));
    return dir.exists() && dir.removeRecursively();
}
