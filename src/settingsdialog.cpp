#include "settingsdialog.h"

#include "imageprovider.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QToolButton>
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
    // A text button rather than an icon: Windows has no icon theme to supply an eye glyph.
    auto *revealButton = new QToolButton(this);
    revealButton->setText(tr("Show"));
    revealButton->setCheckable(true);
    connect(revealButton, &QToolButton::toggled, this, [this, revealButton](bool shown) {
        m_apiKeyEdit->setEchoMode(shown ? QLineEdit::Normal : QLineEdit::Password);
        revealButton->setText(shown ? tr("Hide") : tr("Show"));
    });
    auto *keyRow = new QHBoxLayout;
    keyRow->addWidget(m_apiKeyEdit, 1);
    keyRow->addWidget(revealButton);

    m_modelCombo = new QComboBox(this);
    m_modelCombo->setEditable(true);
    m_modelCombo->setInsertPolicy(QComboBox::NoInsert);
    m_envHint = new QLabel(this);
    m_envHint->setWordWrap(true);
    m_envHint->setStyleSheet(QStringLiteral("color: palette(mid);"));

    m_testButton = new QPushButton(tr("Test connection"), this);
    m_testResult = new QLabel(this);
    m_testResult->setWordWrap(true);
    m_testResult->setTextInteractionFlags(Qt::TextSelectableByMouse);

    auto *form = new QFormLayout;
    form->addRow(tr("Provider:"), m_providerCombo);
    form->addRow(tr("API key:"), keyRow);
    form->addRow(QString(), m_envHint);
    form->addRow(tr("Model:"), m_modelCombo);

    auto *testRow = new QHBoxLayout;
    testRow->addWidget(m_testButton);
    testRow->addStretch();

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addLayout(testRow);
    layout->addWidget(m_testResult);
    layout->addWidget(buttons);

    connect(m_providerCombo, &QComboBox::currentIndexChanged, this, [this] {
        storeFields(m_shownKind);
        // Drop any in-flight test so its result can't land under the other provider.
        m_testProvider.reset();
        m_testButton->setEnabled(true);
        m_shownKind = currentKind();
        loadFields(m_shownKind);
    });
    connect(m_testButton, &QPushButton::clicked, this, &SettingsDialog::testConnection);
    connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &SettingsDialog::reject);

    loadFields(m_shownKind);
}

SettingsDialog::~SettingsDialog() = default;

void SettingsDialog::resizeEvent(QResizeEvent *event)
{
    QDialog::resizeEvent(event);
    fitHeight();
}

void SettingsDialog::fitHeight()
{
    // Nothing here benefits from extra height, so only the width is resizable. The
    // wrapped labels make the right height depend on the width, hence height-for-width.
    const int h = layout()->totalHeightForWidth(width());
    if (h > 0 && h != height())
        setFixedHeight(h);
}

ProviderKind SettingsDialog::currentKind() const
{
    return ProviderKind(m_providerCombo->currentData().toInt());
}

void SettingsDialog::storeFields(ProviderKind kind)
{
    m_fields[int(kind)] = {m_apiKeyEdit->text().trimmed(), m_modelCombo->currentText().trimmed()};
}

void SettingsDialog::loadFields(ProviderKind kind)
{
    const ProviderFields fields = m_fields.value(int(kind));
    m_apiKeyEdit->setText(fields.apiKey);
    m_modelCombo->clear();
    m_modelCombo->addItems(AppSettings::suggestedModels(kind));
    m_modelCombo->setCurrentText(fields.model);

    const QString envVar = AppSettings::apiKeyEnvVar(kind);
    const bool envSet = !qEnvironmentVariable(envVar.toLatin1().constData()).trimmed().isEmpty();
    m_envHint->setText(envSet ? tr("Leave empty to use %1 (currently set).").arg(envVar)
                              : tr("Leave empty to use the %1 environment variable (not set).")
                                    .arg(envVar));
    m_testResult->clear();
    m_testResult->hide();
    fitHeight();
}

void SettingsDialog::testConnection()
{
    const ProviderKind kind = currentKind();
    QString key = m_apiKeyEdit->text().trimmed();
    if (key.isEmpty())
        key = qEnvironmentVariable(AppSettings::apiKeyEnvVar(kind).toLatin1().constData()).trimmed();
    QString model = m_modelCombo->currentText().trimmed();
    if (model.isEmpty())
        model = AppSettings::defaultModel(kind);

    m_testProvider = ImageProvider::create(kind, m_network, key, model);
    m_testButton->setEnabled(false);
    m_testResult->setStyleSheet(QString());
    m_testResult->setText(tr("Testing…"));
    m_testResult->show();
    fitHeight();
    connect(m_testProvider.get(), &ImageProvider::connectionTested, this,
            [this](bool ok, const QString &message) {
                m_testButton->setEnabled(true);
                // Mid-saturation colours that stay legible on light and dark palettes.
                m_testResult->setStyleSheet(ok ? QStringLiteral("color: #2e9e4f;")
                                               : QStringLiteral("color: #d64545;"));
                m_testResult->setText((ok ? QStringLiteral("✔ ") : QStringLiteral("✘ ")) + message);
                fitHeight();
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
