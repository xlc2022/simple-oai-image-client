#include "mainwindow.h"

#include "settingsdialog.h"

#include <QApplication>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QStatusBar>
#include <QTimer>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_network(new QNetworkAccessManager(this))
{
    setWindowTitle(QApplication::applicationDisplayName());
    resize(960, 680);

    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(tr("&Settings…"), QKeySequence::Preferences, this, &MainWindow::openSettings);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("&Quit"), QKeySequence::Quit, qApp, &QApplication::quit);

    QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(tr("&About"), this, [this] {
        QMessageBox::about(this, tr("About"),
                           tr("<b>%1</b> %2<br>Generate and edit images with OpenAI's image models.")
                               .arg(QApplication::applicationDisplayName(),
                                    QApplication::applicationVersion()));
    });

    m_placeholder = new QLabel(this);
    m_placeholder->setAlignment(Qt::AlignCenter);
    m_placeholder->setWordWrap(true);
    setCentralWidget(m_placeholder);

    m_providerStatus = new QLabel(this);
    statusBar()->addPermanentWidget(m_providerStatus);

    refreshStatus();
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
        refreshStatus();
}

void MainWindow::refreshStatus()
{
    const ProviderKind kind = m_settings.provider();
    const bool hasKey = !m_settings.effectiveApiKey(kind).isEmpty();
    const QString providerName = kind == ProviderKind::OpenRouter ? tr("OpenRouter") : tr("OpenAI");

    m_providerStatus->setText(tr("%1 · %2").arg(providerName, m_settings.model(kind)));
    m_placeholder->setText(hasKey ? tr("Ready. Image generation and editing will appear here.")
                                  : tr("Add your API key in File → Settings to get started."));
}
