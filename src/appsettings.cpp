#include "appsettings.h"

#include <QSettings>

namespace {
QString groupFor(ProviderKind kind)
{
    return providerKindToString(kind);
}
} // namespace

QString providerKindToString(ProviderKind kind)
{
    switch (kind) {
    case ProviderKind::OpenAI:
        return QStringLiteral("openai");
    case ProviderKind::OpenRouter:
        return QStringLiteral("openrouter");
    }
    return QStringLiteral("openai");
}

ProviderKind providerKindFromString(const QString &value)
{
    if (value == QLatin1String("openrouter"))
        return ProviderKind::OpenRouter;
    return ProviderKind::OpenAI;
}

ProviderKind AppSettings::provider() const
{
    QSettings s;
    return providerKindFromString(s.value(QStringLiteral("provider")).toString());
}

void AppSettings::setProvider(ProviderKind kind)
{
    QSettings s;
    s.setValue(QStringLiteral("provider"), providerKindToString(kind));
}

QString AppSettings::apiKey(ProviderKind kind) const
{
    QSettings s;
    return s.value(groupFor(kind) + QStringLiteral("/apiKey")).toString();
}

void AppSettings::setApiKey(ProviderKind kind, const QString &key)
{
    QSettings s;
    s.setValue(groupFor(kind) + QStringLiteral("/apiKey"), key.trimmed());
}

QString AppSettings::model(ProviderKind kind) const
{
    QSettings s;
    const QString stored = s.value(groupFor(kind) + QStringLiteral("/model")).toString();
    return stored.isEmpty() ? defaultModel(kind) : stored;
}

void AppSettings::setModel(ProviderKind kind, const QString &model)
{
    QSettings s;
    s.setValue(groupFor(kind) + QStringLiteral("/model"), model.trimmed());
}

QString AppSettings::defaultModel(ProviderKind kind)
{
    switch (kind) {
    case ProviderKind::OpenAI:
        return QStringLiteral("gpt-image-1");
    case ProviderKind::OpenRouter:
        return QStringLiteral("openai/gpt-image-1");
    }
    return {};
}

QString AppSettings::apiKeyEnvVar(ProviderKind kind)
{
    switch (kind) {
    case ProviderKind::OpenAI:
        return QStringLiteral("OPENAI_API_KEY");
    case ProviderKind::OpenRouter:
        return QStringLiteral("OPENROUTER_API_KEY");
    }
    return {};
}

QString AppSettings::effectiveApiKey(ProviderKind kind) const
{
    const QString stored = apiKey(kind);
    if (!stored.isEmpty())
        return stored;
    return qEnvironmentVariable(apiKeyEnvVar(kind).toLatin1().constData()).trimmed();
}
