#pragma once

#include <QImage>
#include <QList>
#include <QNetworkRequest>
#include <QObject>
#include <QPointer>
#include <QString>

#include <memory>

#include "appsettings.h"

class QNetworkAccessManager;
class QNetworkReply;

// What the user asked for on the Generate screen.
struct GenerateOptions {
    QString prompt;
    QString size = QStringLiteral("auto");    // "auto" or "WIDTHxHEIGHT", e.g. "1024x1024"
    QString quality = QStringLiteral("auto"); // "auto", "low", "medium" or "high"
    int count = 1;
};

// Edit an existing image; transparent pixels in `mask` (same size as `image`)
// mark the area to change. A null mask lets the model change anything.
struct EditOptions : GenerateOptions {
    QImage image;
    QImage mask;
};

// A request body with its Content-Type.
struct EncodedBody {
    QByteArray data;
    QByteArray contentType;
};

// Images decoded from a response, or the reason there are none.
struct ImageResult {
    QList<QImage> images;
    QString error;
};

// Common interface for the image backends. Each provider knows its base URL,
// how to authenticate, and how to check that the configured key works.
class ImageProvider : public QObject
{
    Q_OBJECT

public:
    ImageProvider(QNetworkAccessManager *network, QString apiKey, QString model,
                  QObject *parent = nullptr);

    virtual QString displayName() const = 0;
    virtual QUrl baseUrl() const = 0;

    QString apiKey() const { return m_apiKey; }
    QString model() const { return m_model; }

    // A request to `path` (relative to baseUrl) with auth headers set.
    virtual QNetworkRequest makeRequest(const QString &path) const;

    // Request used to verify the key; overridden per provider.
    virtual QNetworkRequest connectionTestRequest() const = 0;

    // Sends connectionTestRequest() and emits connectionTested().
    void testConnection();

    // HTTP request and JSON body for an image generation call.
    virtual QNetworkRequest generateRequest() const = 0;
    // Whether GenerateOptions::quality is sent to the API.
    virtual bool supportsQuality() const { return true; }
    virtual QByteArray generateBody(const GenerateOptions &options) const = 0;

    // HTTP request and body for an image edit call.
    virtual QNetworkRequest editRequest() const = 0;
    virtual EncodedBody editBody(const EditOptions &options) const = 0;
    // Whether EditOptions::mask is sent to the API.
    virtual bool supportsMask() const { return true; }

    // Decodes a successful (HTTP 2xx) generation or edit response.
    virtual ImageResult parseImageResponse(const QByteArray &body) const = 0;

    // Send a request; each emits imagesReady() or requestFailed().
    // Only one request runs at a time; a new one cancels the previous.
    void generate(const GenerateOptions &options);
    void edit(const EditOptions &options);
    void cancel();
    bool isBusy() const { return !m_pending.isNull(); }

    static std::unique_ptr<ImageProvider> create(ProviderKind kind,
                                                 QNetworkAccessManager *network,
                                                 const QString &apiKey,
                                                 const QString &model);

    // Longest side, in pixels, of images uploaded for editing. Larger photos are
    // scaled down first: the models' output is at most 1536 px anyway.
    static constexpr int kMaxUploadSide = 2048;
    // `image` scaled to fit kMaxUploadSide, encoded as PNG.
    static QByteArray encodeForUpload(const QImage &image);

signals:
    void connectionTested(bool ok, const QString &message);
    void imagesReady(const QList<QImage> &images);
    void requestFailed(const QString &message);

protected:
    void send(QNetworkRequest request, const EncodedBody &body);

    // Best-effort human-readable error from an API error reply.
    static QString errorMessage(QNetworkReply *reply, const QByteArray &body);

    QNetworkAccessManager *m_network;
    QString m_apiKey;
    QString m_model;
    QPointer<QNetworkReply> m_pending;
};
