#pragma once

#include <QMainWindow>

#include "appsettings.h"

class QLabel;
class QNetworkAccessManager;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

    // Opens the settings dialog if no API key is configured yet.
    void promptForSetupIfNeeded();

private:
    void openSettings();
    void refreshStatus();

    AppSettings m_settings;
    QNetworkAccessManager *m_network;
    QLabel *m_placeholder;
    QLabel *m_providerStatus;
};
