#pragma once

#include <QMainWindow>

#include <memory>

#include "appsettings.h"

class QLabel;
class QNetworkAccessManager;
class GeneratePanel;
class ImageProvider;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    // Opens the settings dialog if no API key is configured yet.
    void promptForSetupIfNeeded();

private:
    void openSettings();
    // Rebuilds the provider from saved settings and updates the status bar.
    void applySettings();

    AppSettings m_settings;
    QNetworkAccessManager *m_network;
    std::unique_ptr<ImageProvider> m_provider;
    GeneratePanel *m_generatePanel;
    QLabel *m_providerStatus;
};
