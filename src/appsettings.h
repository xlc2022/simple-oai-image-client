#pragma once

#include <QString>
#include <QStringList>

// Which backend the app talks to.
enum class ProviderKind { OpenAI, OpenRouter };

QString providerKindToString(ProviderKind kind);
ProviderKind providerKindFromString(const QString &value);

// Persistent user settings, stored with QSettings (registry on Windows,
// ~/.config on Linux). The API key is stored in plain text there; if it is
// empty, the provider's environment variable is used instead.
class AppSettings
{
public:
    ProviderKind provider() const;
    void setProvider(ProviderKind kind);

    QString apiKey(ProviderKind kind) const;
    void setApiKey(ProviderKind kind, const QString &key);

    QString model(ProviderKind kind) const;
    void setModel(ProviderKind kind, const QString &model);

    static QString defaultModel(ProviderKind kind);
    // Common image models offered in the Settings model picker; any id can still be typed.
    static QStringList suggestedModels(ProviderKind kind);
    static QString apiKeyEnvVar(ProviderKind kind);

    // Stored key, or the environment variable when nothing is stored.
    QString effectiveApiKey(ProviderKind kind) const;
};
