#include "settingsdialog.h"

#include "imageprovider.h"

#include <QAction>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

SettingsDialog::SettingsDialog(QNetworkAccessManager *network, QWidget *parent)
    : QDialog(parent), m_network(network), m_shownKind(m_settings.provider())
{
    setWindowTitle(tr("Settings"));
    setMinimumWidth(460);

    for (ProviderKind kind : {ProviderKind::OpenAI, ProviderKind::OpenRouter})
        m_fields.insert(int(kind), {m_settings.apiKey(kind), m_settings.model(kind)});

    m_providerCombo = new QComboBox(this);
    m_providerCombo->addItem(tr("OpenAI"), int(ProviderKind::OpenAI));
    m_providerCombo->addItem(tr("OpenRouter"), int(ProviderKind::OpenRouter));
    m_providerCombo->setCurrentIndex(m_providerCombo->findData(int(m_shownKind)));

    m_apiKeyEdit = new QLineEdit(this);
    m_apiKeyEdit->setEchoMode(QLineEdit::Password);
    QAction *revealAction = m_apiKeyEdit->addAction(
        style()->standardIcon(QStyle::SP_FileDialogContentsView), QLineEdit::TrailingPosition);
    revealAction->setCheckable(true);
    revealAction->setToolTip(tr("Show key"));
    connect(revealAction, &QAction::toggled, this, [this](bool shown) {
        m_apiKeyEdit->setEchoMode(shown ? QLineEdit::Normal : QLineEdit::Password);
    });

    m_modelEdit = new QLineEdit(this);
    m_envHint = new QLabel(this);
    m_envHint->setWordWrap(true);
    m_envHint->setStyleSheet(QStringLiteral("color: palette(mid);"));

    m_testButton = new QPushButton(tr("Test connection"), this);
    m_testResult = new QLabel(this);
    m_testResult->setWordWrap(true);
    m_testResult->setTextInteractionFlags(Qt::TextSelectableByMouse);

    auto *form = new QFormLayout;
    form->addRow(tr("Provider:"), m_providerCombo);
    form->addRow(tr("API key:"), m_apiKeyEdit);
    form->addRow(QString(), m_envHint);
    form->addRow(tr("Model:"), m_modelEdit);

    auto *testRow = new QHBoxLayout;
    testRow->addWidget(m_testButton);
    testRow->addWidget(m_testResult, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addLayout(testRow);
    layout->addStretch();
    layout->addWidget(buttons);

    connect(m_providerCombo, &QComboBox::currentIndexChanged, this, [this] {
        storeFields(m_shownKind);
        m_shownKind = currentKind();
        loadFields(m_shownKind);
    });
    connect(m_testButton, &QPushButton::clicked, this, &SettingsDialog::testConnection);
    connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &SettingsDialog::reject);

    loadFields(m_shownKind);
}

SettingsDialog::~SettingsDialog() = default;

ProviderKind SettingsDialog::currentKind() const
{
    return ProviderKind(m_providerCombo->currentData().toInt());
}

void SettingsDialog::storeFields(ProviderKind kind)
{
    m_fields[int(kind)] = {m_apiKeyEdit->text().trimmed(), m_modelEdit->text().trimmed()};
}

void SettingsDialog::loadFields(ProviderKind kind)
{
    const ProviderFields fields = m_fields.value(int(kind));
    m_apiKeyEdit->setText(fields.apiKey);
    m_modelEdit->setText(fields.model);
    m_modelEdit->setPlaceholderText(AppSettings::defaultModel(kind));
    m_envHint->setText(tr("Leave empty to use the %1 environment variable.")
                           .arg(AppSettings::apiKeyEnvVar(kind)));
    m_testResult->clear();
}

void SettingsDialog::testConnection()
{
    const ProviderKind kind = currentKind();
    QString key = m_apiKeyEdit->text().trimmed();
    if (key.isEmpty())
        key = qEnvironmentVariable(AppSettings::apiKeyEnvVar(kind).toLatin1().constData()).trimmed();
    QString model = m_modelEdit->text().trimmed();
    if (model.isEmpty())
        model = AppSettings::defaultModel(kind);

    m_testProvider = ImageProvider::create(kind, m_network, key, model);
    m_testButton->setEnabled(false);
    m_testResult->setText(tr("Testing…"));
    connect(m_testProvider.get(), &ImageProvider::connectionTested, this,
            [this](bool ok, const QString &message) {
                m_testButton->setEnabled(true);
                m_testResult->setText((ok ? QStringLiteral("✔ ") : QStringLiteral("✘ ")) + message);
            });
    m_testProvider->testConnection();
}

void SettingsDialog::accept()
{
    storeFields(m_shownKind);
    m_settings.setProvider(currentKind());
    for (auto it = m_fields.cbegin(); it != m_fields.cend(); ++it) {
        m_settings.setApiKey(ProviderKind(it.key()), it->apiKey);
        m_settings.setModel(ProviderKind(it.key()), it->model);
    }
    QDialog::accept();
}
