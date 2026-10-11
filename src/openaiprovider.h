#pragma once

#include "imageprovider.h"

// OpenAI's own API (api.openai.com), using the Images endpoints.
class OpenAIProvider : public ImageProvider
{
    Q_OBJECT

public:
    using ImageProvider::ImageProvider;

    QString displayName() const override { return QStringLiteral("OpenAI"); }
    QUrl baseUrl() const override { return QUrl(QStringLiteral("https://api.openai.com/v1")); }
    QNetworkRequest connectionTestRequest() const override;

    QNetworkRequest generateRequest() const override;
    QByteArray generateBody(const GenerateOptions &options) const override;
    ImageResult parseGenerateResponse(const QByteArray &body) const override;
};
