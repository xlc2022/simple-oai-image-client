#pragma once

#include <QDialog>
#include <QHash>

#include <memory>

#include "appsettings.h"

class QComboBox;
class QLabel;
class QLineEdit;
class QNetworkAccessManager;
class QPushButton;
class ImageProvider;

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QNetworkAccessManager *network, QWidget *parent = nullptr);
    ~SettingsDialog() override;

    void accept() override;

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    struct ProviderFields {
        QString apiKey;
        QString model;
    };

    ProviderKind currentKind() const;
    void storeFields(ProviderKind kind);
    void loadFields(ProviderKind kind);
    void testConnection();
    void fitHeight();

    QNetworkAccessManager *m_network;
    AppSettings m_settings;
    QHash<int, ProviderFields> m_fields;
    ProviderKind m_shownKind;

    QComboBox *m_providerCombo;
    QLineEdit *m_apiKeyEdit;
    QComboBox *m_modelCombo;
    QLabel *m_envHint;
    QPushButton *m_testButton;
    QLabel *m_testResult;
    std::unique_ptr<ImageProvider> m_testProvider;
};
