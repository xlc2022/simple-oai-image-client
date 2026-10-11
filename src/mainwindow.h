#pragma once

#include <QMainWindow>

#include <memory>

#include "appsettings.h"
#include "historystore.h"

class QLabel;
class QNetworkAccessManager;
class HistoryPanel;
class ImagePanel;
class QTabWidget;
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
    // One provider per panel so each panel only sees its own results.
    std::unique_ptr<ImageProvider> m_generateProvider;
    std::unique_ptr<ImageProvider> m_editProvider;
    QTabWidget *m_tabs;
    ImagePanel *m_generatePanel;
    ImagePanel *m_editPanel;
    HistoryStore m_history;
    HistoryPanel *m_historyPanel;
    QLabel *m_providerStatus;
};
