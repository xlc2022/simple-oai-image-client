#include "openrouterprovider.h"

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
