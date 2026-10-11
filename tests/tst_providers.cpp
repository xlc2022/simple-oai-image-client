#include <QCoreApplication>
#include <QNetworkAccessManager>
#include <QSettings>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>

#include "appsettings.h"
#include "imageprovider.h"

class TestProviders : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setOrganizationName(QStringLiteral("SimpleImageClientTests"));
        QCoreApplication::setApplicationName(QStringLiteral("tst_providers"));
        QSettings().clear();
    }

    void openAiRequest()
    {
        QNetworkAccessManager nam;
        auto provider = ImageProvider::create(ProviderKind::OpenAI, &nam, "sk-test", "gpt-image-1");
        const QNetworkRequest request = provider->connectionTestRequest();
        QCOMPARE(request.url(), QUrl("https://api.openai.com/v1/models/gpt-image-1"));
        QCOMPARE(request.rawHeader("Authorization"), QByteArray("Bearer sk-test"));
    }

    void openRouterRequest()
    {
        QNetworkAccessManager nam;
        auto provider = ImageProvider::create(ProviderKind::OpenRouter, &nam, "sk-or-test",
                                              "openai/gpt-image-1");
        const QNetworkRequest request = provider->connectionTestRequest();
        QCOMPARE(request.url(), QUrl("https://openrouter.ai/api/v1/key"));
        QCOMPARE(request.rawHeader("Authorization"), QByteArray("Bearer sk-or-test"));
        QVERIFY(request.hasRawHeader("X-Title"));
    }

    void missingKeyFailsWithoutNetwork()
    {
        QNetworkAccessManager nam;
        auto provider = ImageProvider::create(ProviderKind::OpenAI, &nam, QString(), "gpt-image-1");
        QSignalSpy spy(provider.get(), &ImageProvider::connectionTested);
        provider->testConnection();
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toBool(), false);
    }

    void settingsRoundTrip()
    {
        AppSettings settings;
        QCOMPARE(settings.provider(), ProviderKind::OpenAI);
        QCOMPARE(settings.model(ProviderKind::OpenRouter), QString("openai/gpt-image-1"));

        settings.setProvider(ProviderKind::OpenRouter);
        settings.setApiKey(ProviderKind::OpenRouter, "  key-with-spaces  ");
        settings.setModel(ProviderKind::OpenRouter, "openai/gpt-image-2");

        QCOMPARE(settings.provider(), ProviderKind::OpenRouter);
        QCOMPARE(settings.apiKey(ProviderKind::OpenRouter), QString("key-with-spaces"));
        QCOMPARE(settings.model(ProviderKind::OpenRouter), QString("openai/gpt-image-2"));
        QVERIFY(settings.apiKey(ProviderKind::OpenAI).isEmpty());
    }

    void envVarFallback()
    {
        AppSettings settings;
        settings.setApiKey(ProviderKind::OpenAI, QString());
        qputenv("OPENAI_API_KEY", "from-env");
        QCOMPARE(settings.effectiveApiKey(ProviderKind::OpenAI), QString("from-env"));
        settings.setApiKey(ProviderKind::OpenAI, "stored");
        QCOMPARE(settings.effectiveApiKey(ProviderKind::OpenAI), QString("stored"));
        qunsetenv("OPENAI_API_KEY");
    }

    void cleanupTestCase() { QSettings().clear(); }
};

QTEST_GUILESS_MAIN(TestProviders)
#include "tst_providers.moc"
