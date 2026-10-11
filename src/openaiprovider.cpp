#include "openaiprovider.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

QNetworkRequest OpenAIProvider::connectionTestRequest() const
{
    // Retrieving the configured model checks both the key and model access.
    return makeRequest(QStringLiteral("/models/") + model());
}

QNetworkRequest OpenAIProvider::generateRequest() const
{
    return makeRequest(QStringLiteral("/images/generations"));
}

QByteArray OpenAIProvider::generateBody(const GenerateOptions &options) const
{
    QJsonObject body{
        {"model", model()},
        {"prompt", options.prompt},
        {"n", qMax(1, options.count)},
        {"size", options.size},
        {"quality", options.quality},
    };
    return QJsonDocument(body).toJson(QJsonDocument::Compact);
}

ImageResult OpenAIProvider::parseGenerateResponse(const QByteArray &body) const
{
    // gpt-image models always return base64 data: {"data": [{"b64_json": "..."}]}
    ImageResult result;
    const QJsonArray data = QJsonDocument::fromJson(body).object().value("data").toArray();
    for (const QJsonValue &item : data) {
        const QByteArray b64 = item.toObject().value("b64_json").toString().toLatin1();
        QImage image;
        if (!b64.isEmpty() && image.loadFromData(QByteArray::fromBase64(b64)))
            result.images.append(image);
    }
    if (result.images.isEmpty())
        result.error = tr("The response contained no images.");
    return result;
}
