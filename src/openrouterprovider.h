#pragma once

#include "imageprovider.h"

// OpenRouter (openrouter.ai). It serves OpenAI's image models such as
// openai/gpt-image-1 through its chat completions API with image output.
class OpenRouterProvider : public ImageProvider
{
    Q_OBJECT

public:
    using ImageProvider::ImageProvider;

    QString displayName() const override { return QStringLiteral("OpenRouter"); }
    QUrl baseUrl() const override { return QUrl(QStringLiteral("https://openrouter.ai/api/v1")); }
    QNetworkRequest makeRequest(const QString &path) const override;
    QNetworkRequest connectionTestRequest() const override;
};
