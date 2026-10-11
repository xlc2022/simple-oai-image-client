#pragma once

#include <QObject>
#include <QNetworkRequest>
#include <QString>

#include <memory>

#include "appsettings.h"

class QNetworkAccessManager;
class QNetworkReply;

// Common interface for the image backends. Each provider knows its base URL,
// how to authenticate, and how to check that the configured key works.
class ImageProvider : public QObject
{
    Q_OBJECT

public:
    ImageProvider(QNetworkAccessManager *network, QString apiKey, QString model,
                  QObject *parent = nullptr);

    virtual QString displayName() const = 0;
    virtual QUrl baseUrl() const = 0;

    QString apiKey() const { return m_apiKey; }
    QString model() const { return m_model; }

    // A request to `path` (relative to baseUrl) with auth headers set.
    virtual QNetworkRequest makeRequest(const QString &path) const;

    // Request used to verify the key; overridden per provider.
    virtual QNetworkRequest connectionTestRequest() const = 0;

    // Sends connectionTestRequest() and emits connectionTested().
    void testConnection();

    static std::unique_ptr<ImageProvider> create(ProviderKind kind,
                                                 QNetworkAccessManager *network,
                                                 const QString &apiKey,
                                                 const QString &model);

signals:
    void connectionTested(bool ok, const QString &message);

protected:
    // Best-effort human-readable error from an API error reply.
    static QString errorMessage(QNetworkReply *reply, const QByteArray &body);

    QNetworkAccessManager *m_network;
    QString m_apiKey;
    QString m_model;
};
