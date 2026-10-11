#include "openaiprovider.h"

QNetworkRequest OpenAIProvider::connectionTestRequest() const
{
    // Retrieving the configured model checks both the key and model access.
    return makeRequest(QStringLiteral("/models/") + model());
}
