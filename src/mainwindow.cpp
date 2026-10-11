#include "mainwindow.h"

#include "historypanel.h"
#include "imagepanel.h"
#include "imageprovider.h"
#include "settingsdialog.h"

#include <QAction>
#include <QApplication>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QStatusBar>
#include <QTabWidget>
#include <QTimer>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_network(new QNetworkAccessManager(this))
{
    setWindowTitle(QApplication::applicationDisplayName());
    resize(960, 680);

    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    // The standard Preferences/Quit keys are unbound on Windows (and Preferences on GNOME).
    QKeySequence prefsKey(QKeySequence::Preferences);
    if (prefsKey.isEmpty())
        prefsKey = QKeySequence(QStringLiteral("Ctrl+,"));
    QKeySequence quitKey(QKeySequence::Quit);
    if (quitKey.isEmpty())
        quitKey = QKeySequence(QStringLiteral("Ctrl+Q"));

    QAction *settingsAction =
        fileMenu->addAction(tr("&Settings…"), prefsKey, this, &MainWindow::openSettings);
    settingsAction->setMenuRole(QAction::PreferencesRole);
    fileMenu->addSeparator();
    QAction *quitAction = fileMenu->addAction(tr("&Quit"), quitKey, qApp, &QApplication::quit);
    quitAction->setMenuRole(QAction::QuitRole);

    QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(tr("&About"), this, [this] {
        QMessageBox::about(this, tr("About"),
                           tr("<b>%1</b> %2<br>Generate and edit images with OpenAI's image models.")
                               .arg(QApplication::applicationDisplayName(),
                                    QApplication::applicationVersion()));
    });

    m_tabs = new QTabWidget(this);
    m_tabs->setDocumentMode(true);
    m_generatePanel = new ImagePanel(ImagePanel::Mode::Generate, m_tabs);
    m_editPanel = new ImagePanel(ImagePanel::Mode::Edit, m_tabs);
    m_tabs->addTab(m_generatePanel, tr("&Generate"));
    m_tabs->addTab(m_editPanel, tr("&Edit"));
    m_historyPanel = new HistoryPanel(&m_history, m_tabs);
    m_tabs->addTab(m_historyPanel, tr("&History"));
    setCentralWidget(m_tabs);

    auto openInEdit = [this](const QImage &image) {
        m_editPanel->setSourceImage(image);
        m_tabs->setCurrentWidget(m_editPanel);
    };
    connect(m_generatePanel, &ImagePanel::editRequested, this, openInEdit);
    connect(m_historyPanel, &HistoryPanel::editRequested, this, openInEdit);

    // Every finished request goes into History.
    auto record = [this](const HistoryEntry &entry, const QList<QImage> &images,
                         const QImage &source) {
        const HistoryEntry stored = m_history.add(entry, images, source);
        if (stored.id.isEmpty())
            statusBar()->showMessage(tr("Couldn't save this result to History (%1).")
                                         .arg(m_history.root()),
                                     8000);
        else
            m_historyPanel->addEntry(stored);
    };
    connect(m_generatePanel, &ImagePanel::completed, this, record);
    connect(m_editPanel, &ImagePanel::completed, this, record);

    connect(m_historyPanel, &HistoryPanel::reuseRequested, this, [this](const HistoryEntry &entry) {
        ImagePanel *panel = entry.isEdit ? m_editPanel : m_generatePanel;
        if (entry.isEdit && !entry.sourceFile.isEmpty())
            m_editPanel->setSourceImage(QImage(entry.sourceFile));
        panel->loadRequest(entry);
        m_tabs->setCurrentWidget(panel);
    });

    m_providerStatus = new QLabel(this);
    statusBar()->addPermanentWidget(m_providerStatus);

    applySettings();
}

MainWindow::~MainWindow()
{
    // The panels hold QPointers to the providers; detach before the providers go away.
    m_generatePanel->setProvider(nullptr);
    m_editPanel->setProvider(nullptr);
}

void MainWindow::promptForSetupIfNeeded()
{
    if (m_settings.effectiveApiKey(m_settings.provider()).isEmpty())
        QTimer::singleShot(0, this, &MainWindow::openSettings);
}

void MainWindow::openSettings()
{
    SettingsDialog dialog(m_network, this);
    if (dialog.exec() == QDialog::Accepted)
        applySettings();
}

void MainWindow::applySettings()
{
    const ProviderKind kind = m_settings.provider();
    const QString key = m_settings.effectiveApiKey(kind);

    m_generatePanel->setProvider(nullptr);
    m_editPanel->setProvider(nullptr);
    if (key.isEmpty()) {
        m_generateProvider.reset();
        m_editProvider.reset();
    } else {
        m_generateProvider = ImageProvider::create(kind, m_network, key, m_settings.model(kind));
        m_editProvider = ImageProvider::create(kind, m_network, key, m_settings.model(kind));
    }
    m_generatePanel->setProvider(m_generateProvider.get());
    m_editPanel->setProvider(m_editProvider.get());

    const QString providerName = kind == ProviderKind::OpenRouter ? tr("OpenRouter") : tr("OpenAI");
    m_providerStatus->setText(key.isEmpty()
                                  ? tr("%1 · no API key").arg(providerName)
                                  : tr("%1 · %2").arg(providerName, m_settings.model(kind)));
}
