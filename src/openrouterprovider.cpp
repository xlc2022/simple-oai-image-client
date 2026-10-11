#include "openrouterprovider.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <numeric>

namespace {
// OpenRouter takes an aspect ratio rather than pixel dimensions.
QString aspectRatioForSize(const QString &size)
{
    const QStringList parts = size.split(QLatin1Char('x'));
    if (parts.size() != 2)
        return {};
    const int w = parts[0].toInt();
    const int h = parts[1].toInt();
    if (w <= 0 || h <= 0)
        return {};
    const int g = std::gcd(w, h);
    return QStringLiteral("%1:%2").arg(w / g).arg(h / g);
}
} // namespace

QNetworkRequest OpenRouterProvider::makeRequest(const QString &path) const
{
    QNetworkRequest request = ImageProvider::makeRequest(path);
    // Optional attribution headers OpenRouter uses to identify the app.
    request.setRawHeader("X-Title", "Simple Image Client");
    return request;
}

QNetworkRequest OpenRouterProvider::connectionTestRequest() const
{
    // Returns details about the key itself; fails with 401 for a bad key.
    return makeRequest(QStringLiteral("/key"));
}

QNetworkRequest OpenRouterProvider::generateRequest() const
{
    return makeRequest(QStringLiteral("/chat/completions"));
}

QByteArray OpenRouterProvider::generateBody(const GenerateOptions &options) const
{
    return chatBody(options, options.prompt);
}

QNetworkRequest OpenRouterProvider::editRequest() const
{
    // Edits are a chat message carrying the source image alongside the prompt.
    return makeRequest(QStringLiteral("/chat/completions"));
}

EncodedBody OpenRouterProvider::editBody(const EditOptions &options) const
{
    const QString dataUrl = QStringLiteral("data:image/png;base64,")
                            + QString::fromLatin1(encodeForUpload(options.image).toBase64());
    const QJsonArray content{
        QJsonObject{{"type", "text"}, {"text", options.prompt}},
        QJsonObject{{"type", "image_url"}, {"image_url", QJsonObject{{"url", dataUrl}}}},
    };
    return {chatBody(options, content), "application/json"};
}

QByteArray OpenRouterProvider::chatBody(const GenerateOptions &options, const QJsonValue &content) const
{
    QJsonObject body{
        {"model", model()},
        {"messages", QJsonArray{QJsonObject{{"role", "user"}, {"content", content}}}},
        {"modalities", QJsonArray{"image", "text"}},
    };
    // Chat completions produce one image per request; extra images use "n" where supported.
    if (options.count > 1)
        body.insert("n", options.count);
    const QString ratio = aspectRatioForSize(options.size);
    if (!ratio.isEmpty())
        body.insert("image_config", QJsonObject{{"aspect_ratio", ratio}});
    return QJsonDocument(body).toJson(QJsonDocument::Compact);
}

ImageResult OpenRouterProvider::parseImageResponse(const QByteArray &body) const
{
    // {"choices": [{"message": {"content": "...", "images": [
    //     {"type": "image_url", "image_url": {"url": "data:image/png;base64,..."}}]}}]}
    ImageResult result;
    QString text;
    const QJsonArray choices = QJsonDocument::fromJson(body).object().value("choices").toArray();
    for (const QJsonValue &choice : choices) {
        const QJsonObject message = choice.toObject().value("message").toObject();
        if (text.isEmpty())
            text = message.value("content").toString().trimmed();
        for (const QJsonValue &entry : message.value("images").toArray()) {
            const QString url = entry.toObject().value("image_url").toObject().value("url").toString();
            const qsizetype comma = url.indexOf(QLatin1Char(','));
            if (!url.startsWith(QLatin1String("data:")) || comma < 0)
                continue;
            QImage image;
            if (image.loadFromData(QByteArray::fromBase64(url.mid(comma + 1).toLatin1())))
                result.images.append(image);
        }
    }
    if (result.images.isEmpty()) {
        // Models sometimes answer in text (e.g. a refusal) instead of drawing.
        result.error = text.isEmpty() ? tr("The response contained no images.")
                                      : tr("No image returned. The model said: %1").arg(text);
    }
    return result;
}
