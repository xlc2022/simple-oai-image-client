#include "imageprovider.h"

#include "openaiprovider.h"
#include "openrouterprovider.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>

ImageProvider::ImageProvider(QNetworkAccessManager *network, QString apiKey, QString model,
                             QObject *parent)
    : QObject(parent), m_network(network), m_apiKey(std::move(apiKey)), m_model(std::move(model))
{
}

QNetworkRequest ImageProvider::makeRequest(const QString &path) const
{
    QUrl url = baseUrl();
    url.setPath(url.path() + path);
    QNetworkRequest request(url);
    request.setRawHeader("Authorization", "Bearer " + m_apiKey.toUtf8());
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("SimpleImageClient/0.1"));
    return request;
}

void ImageProvider::testConnection()
{
    if (m_apiKey.isEmpty()) {
        emit connectionTested(false, tr("No API key is set."));
        return;
    }

    QNetworkReply *reply = m_network->get(connectionTestRequest());
    // Destroying the provider aborts the request instead of leaving it to finish unobserved.
    reply->setParent(this);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        const QByteArray body = reply->readAll();
        if (reply->error() == QNetworkReply::NoError)
            emit connectionTested(true, tr("Connected to %1.").arg(displayName()));
        else
            emit connectionTested(false, errorMessage(reply, body));
    });
}

QString ImageProvider::errorMessage(QNetworkReply *reply, const QByteArray &body)
{
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    // Both OpenAI and OpenRouter return {"error": {"message": "..."}}.
    const QJsonObject error = QJsonDocument::fromJson(body).object().value("error").toObject();
    const QString apiMessage = error.value("message").toString();

    if (!apiMessage.isEmpty())
        return status ? QStringLiteral("HTTP %1: %2").arg(status).arg(apiMessage) : apiMessage;
    if (status)
        return QStringLiteral("HTTP %1: %2").arg(status).arg(reply->errorString());
    return reply->errorString();
}

std::unique_ptr<ImageProvider> ImageProvider::create(ProviderKind kind,
                                                     QNetworkAccessManager *network,
                                                     const QString &apiKey,
                                                     const QString &model)
{
    switch (kind) {
    case ProviderKind::OpenAI:
        return std::make_unique<OpenAIProvider>(network, apiKey, model);
    case ProviderKind::OpenRouter:
        return std::make_unique<OpenRouterProvider>(network, apiKey, model);
    }
    return nullptr;
}
