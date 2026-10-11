#include <QBuffer>
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QSettings>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>

#include "appsettings.h"
#include "imageprovider.h"

namespace {
QByteArray pngBase64(int w, int h)
{
    QImage image(w, h, QImage::Format_RGB32);
    image.fill(Qt::red);
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return bytes.toBase64();
}
} // namespace

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

    void openAiGenerate()
    {
        QNetworkAccessManager nam;
        auto provider = ImageProvider::create(ProviderKind::OpenAI, &nam, "k", "gpt-image-1");
        QCOMPARE(provider->generateRequest().url(),
                 QUrl("https://api.openai.com/v1/images/generations"));

        GenerateOptions options;
        options.prompt = "a red square";
        options.size = "1024x1536";
        options.quality = "high";
        options.count = 2;
        const QJsonObject body = QJsonDocument::fromJson(provider->generateBody(options)).object();
        QCOMPARE(body["model"].toString(), QString("gpt-image-1"));
        QCOMPARE(body["prompt"].toString(), QString("a red square"));
        QCOMPARE(body["size"].toString(), QString("1024x1536"));
        QCOMPARE(body["quality"].toString(), QString("high"));
        QCOMPARE(body["n"].toInt(), 2);

        const QByteArray response = "{\"data\":[{\"b64_json\":\"" + pngBase64(4, 6)
                                    + "\"},{\"b64_json\":\"" + pngBase64(2, 2) + "\"}]}";
        const ImageResult result = provider->parseGenerateResponse(response);
        QVERIFY(result.error.isEmpty());
        QCOMPARE(result.images.size(), 2);
        QCOMPARE(result.images[0].size(), QSize(4, 6));

        QVERIFY(!provider->parseGenerateResponse("{\"data\":[]}").error.isEmpty());
    }

    void openRouterGenerate()
    {
        QNetworkAccessManager nam;
        auto provider = ImageProvider::create(ProviderKind::OpenRouter, &nam, "k",
                                              "openai/gpt-image-1");
        QCOMPARE(provider->generateRequest().url(),
                 QUrl("https://openrouter.ai/api/v1/chat/completions"));

        GenerateOptions options;
        options.prompt = "a red square";
        options.size = "1536x1024";
        const QJsonObject body = QJsonDocument::fromJson(provider->generateBody(options)).object();
        QCOMPARE(body["model"].toString(), QString("openai/gpt-image-1"));
        QCOMPARE(body["messages"].toArray()[0].toObject()["content"].toString(),
                 QString("a red square"));
        QCOMPARE(body["modalities"].toArray(), (QJsonArray{"image", "text"}));
        QCOMPARE(body["image_config"].toObject()["aspect_ratio"].toString(), QString("3:2"));
        QVERIFY(!body.contains("n"));

        options.size = "auto";
        QVERIFY(!QJsonDocument::fromJson(provider->generateBody(options)).object().contains("image_config"));

        const QByteArray response =
            "{\"choices\":[{\"message\":{\"content\":\"\",\"images\":[{\"type\":\"image_url\","
            "\"image_url\":{\"url\":\"data:image/png;base64," + pngBase64(3, 3) + "\"}}]}}]}";
        const ImageResult result = provider->parseGenerateResponse(response);
        QVERIFY(result.error.isEmpty());
        QCOMPARE(result.images.size(), 1);
        QCOMPARE(result.images[0].size(), QSize(3, 3));

        const ImageResult refusal = provider->parseGenerateResponse(
            "{\"choices\":[{\"message\":{\"content\":\"I can't draw that.\"}}]}");
        QVERIFY(refusal.images.isEmpty());
        QVERIFY(refusal.error.contains("I can't draw that."));
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
