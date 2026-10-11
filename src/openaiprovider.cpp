#include "openaiprovider.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>

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

QNetworkRequest OpenAIProvider::editRequest() const
{
    return makeRequest(QStringLiteral("/images/edits"));
}

EncodedBody OpenAIProvider::editBody(const EditOptions &options) const
{
    // multipart/form-data with the image (and mask) as PNG files.
    const QByteArray boundary =
        "SimpleImageClient-" + QByteArray::number(QRandomGenerator::global()->generate64(), 16);
    QByteArray data;
    auto addPart = [&](const QByteArray &name, const QByteArray &value,
                       const QByteArray &fileName = {}) {
        data += "--" + boundary + "\r\n";
        data += "Content-Disposition: form-data; name=\"" + name + "\"";
        if (!fileName.isEmpty())
            data += "; filename=\"" + fileName + "\"\r\nContent-Type: image/png";
        data += "\r\n\r\n" + value + "\r\n";
    };

    addPart("model", model().toUtf8());
    addPart("prompt", options.prompt.toUtf8());
    addPart("n", QByteArray::number(qMax(1, options.count)));
    addPart("size", options.size.toUtf8());
    addPart("quality", options.quality.toUtf8());
    addPart("image", encodeForUpload(options.image), "image.png");
    if (!options.mask.isNull()) {
        // The mask must match the uploaded image's size, so scale it the same way.
        addPart("mask", encodeForUpload(options.mask.scaled(options.image.size())), "mask.png");
    }
    data += "--" + boundary + "--\r\n";

    return {data, "multipart/form-data; boundary=" + boundary};
}

ImageResult OpenAIProvider::parseImageResponse(const QByteArray &body) const
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
