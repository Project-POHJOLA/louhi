#include "taksettingsdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QFileDialog>
#include <QMessageBox>

TakSettingsDialog::TakSettingsDialog(QWidget* parent)
    : QDialog(parent)
    , m_currentIndex(-1)
{
    setWindowTitle("TAK Communication Settings");
    setMinimumSize(650, 500);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    QHBoxLayout* topLayout = new QHBoxLayout();

    QGroupBox* serverListGroup = new QGroupBox("Servers", this);
    QVBoxLayout* serverListLayout = new QVBoxLayout(serverListGroup);

    m_serverList = new QListWidget(serverListGroup);
    m_serverList->setSelectionMode(QAbstractItemView::SingleSelection);

    QHBoxLayout* listBtnLayout = new QHBoxLayout();
    m_addBtn = new QPushButton("Add", serverListGroup);
    m_removeBtn = new QPushButton("Remove", serverListGroup);
    m_removeBtn->setEnabled(false);
    listBtnLayout->addWidget(m_addBtn);
    listBtnLayout->addWidget(m_removeBtn);
    listBtnLayout->addStretch();

    serverListLayout->addWidget(m_serverList);
    serverListLayout->addLayout(listBtnLayout);

    topLayout->addWidget(serverListGroup, 1);

    QGroupBox* configGroup = new QGroupBox("Server Configuration", this);
    QVBoxLayout* configLayout = new QVBoxLayout(configGroup);

    QFormLayout* formLayout = new QFormLayout();

    m_nameEdit = new QLineEdit(configGroup);
    m_nameEdit->setPlaceholderText("My TAK Server");
    formLayout->addRow("Name:", m_nameEdit);

    m_addressEdit = new QLineEdit(configGroup);
    m_addressEdit->setPlaceholderText("tak.example.com or 192.168.1.100");
    formLayout->addRow("Address:", m_addressEdit);

    m_portSpin = new QSpinBox(configGroup);
    m_portSpin->setRange(1, 65535);
    m_portSpin->setValue(8089);
    formLayout->addRow("Port:", m_portSpin);

    configLayout->addLayout(formLayout);

    QGroupBox* certGroup = new QGroupBox("Client Certificate (PKCS12)", configGroup);
    QVBoxLayout* certLayout = new QVBoxLayout(certGroup);

    QHBoxLayout* certPathLayout = new QHBoxLayout();
    m_certPathEdit = new QLineEdit(certGroup);
    m_certPathEdit->setPlaceholderText("Path to .p12 or .pfx file");
    m_certPathEdit->setReadOnly(true);
    m_browseCertBtn = new QPushButton("Browse...", certGroup);
    certPathLayout->addWidget(m_certPathEdit);
    certPathLayout->addWidget(m_browseCertBtn);
    certLayout->addLayout(certPathLayout);

    m_certPasswordEdit = new QLineEdit(certGroup);
    m_certPasswordEdit->setEchoMode(QLineEdit::Password);
    m_certPasswordEdit->setPlaceholderText("Certificate password (if encrypted)");
    certLayout->addWidget(new QLabel("Password:", certGroup));
    certLayout->addWidget(m_certPasswordEdit);

    configLayout->addWidget(certGroup);

    QGroupBox* identityGroup = new QGroupBox("Identity", configGroup);
    QFormLayout* identityLayout = new QFormLayout(identityGroup);

    m_callsignEdit = new QLineEdit(identityGroup);
    m_callsignEdit->setPlaceholderText("Your callsign");
    identityLayout->addRow("Callsign:", m_callsignEdit);

    m_colorCombo = new QComboBox(identityGroup);
    m_colorCombo->addItems({"White", "Yellow", "Orange", "Magenta", "Red", "Maroon", "Purple", "Dark Blue", "Blue", "Cyan", "Teal", "Green", "Dark Green", "Brown"});
    identityLayout->addRow("Color:", m_colorCombo);

    m_roleCombo = new QComboBox(identityGroup);
    m_roleCombo->addItems({
        "Team Member", "Team Lead", "HQ", "Airborne", "Fixed Wing",
        "Rotary Wing", "Medic", "Forward Observer", "Sniper",
        "RTO", "Corpsman", "Engineer", "Leader", "Point of Contact"
    });
    identityLayout->addRow("Role:", m_roleCombo);

    configLayout->addWidget(identityGroup);

    m_autoConnectCheck = new QCheckBox("Auto-connect on startup", configGroup);
    configLayout->addWidget(m_autoConnectCheck);

    m_debugLoggingCheck = new QCheckBox("Debug logging (print incoming CoT messages)", configGroup);
    configLayout->addWidget(m_debugLoggingCheck);

    topLayout->addWidget(configGroup, 2);
    mainLayout->addLayout(topLayout);

    connect(m_serverList, &QListWidget::currentRowChanged, this, &TakSettingsDialog::onServerSelected);
    connect(m_addBtn, &QPushButton::clicked, this, &TakSettingsDialog::addServer);
    connect(m_removeBtn, &QPushButton::clicked, this, &TakSettingsDialog::removeServer);
    connect(m_browseCertBtn, &QPushButton::clicked, this, &TakSettingsDialog::browseCertFile);

    QHBoxLayout* buttonLayout = new QHBoxLayout();
    QPushButton* okBtn = new QPushButton("OK", this);
    QPushButton* cancelBtn = new QPushButton("Cancel", this);
    buttonLayout->addStretch();
    buttonLayout->addWidget(okBtn);
    buttonLayout->addWidget(cancelBtn);

    mainLayout->addLayout(buttonLayout);

    connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
}

TakSettingsDialog::~TakSettingsDialog()
{
}

void TakSettingsDialog::onServerSelected()
{
    int row = m_serverList->currentRow();
    if (row >= 0 && row < m_servers.size()) {
        if (m_currentIndex >= 0 && m_currentIndex < m_servers.size()) {
            saveFormToCurrentServer();
        }
        m_currentIndex = row;
        loadCurrentServerToForm();
        m_removeBtn->setEnabled(true);
    } else {
        m_removeBtn->setEnabled(false);
    }
}

void TakSettingsDialog::addServer()
{
    if (m_currentIndex >= 0 && m_currentIndex < m_servers.size()) {
        saveFormToCurrentServer();
    }

    TakServerConfig config;
    config.id = QString("server_%1").arg(QDateTime::currentMSecsSinceEpoch());
    config.name = "New Server";
    config.address = "";
    config.port = 8089;
    config.callsign = "Unknown";
    config.color = "Unknown";
    config.role = "Team Member";
    config.autoConnect = false;
    config.debugLogging = false;

    m_servers.append(config);
    m_currentIndex = m_servers.size() - 1;

    updateServerListDisplay();
    m_serverList->setCurrentRow(m_currentIndex);
    loadCurrentServerToForm();
}

void TakSettingsDialog::removeServer()
{
    if (m_currentIndex >= 0 && m_currentIndex < m_servers.size()) {
        m_servers.removeAt(m_currentIndex);
        m_currentIndex = -1;

        updateServerListDisplay();

        if (!m_servers.isEmpty()) {
            m_currentIndex = qMin(m_currentIndex, m_servers.size() - 1);
            m_serverList->setCurrentRow(m_currentIndex);
            loadCurrentServerToForm();
        }

        m_removeBtn->setEnabled(!m_servers.isEmpty());
    }
}

void TakSettingsDialog::browseCertFile()
{
    QString filePath = QFileDialog::getOpenFileName(
        this,
        "Select PKCS12 Certificate",
        QString(),
        "PKCS12 Files (*.p12 *.pfx);;All Files (*)"
    );

    if (!filePath.isEmpty()) {
        m_certPathEdit->setText(filePath);
    }
}

void TakSettingsDialog::loadCurrentServerToForm()
{
    if (m_currentIndex < 0 || m_currentIndex >= m_servers.size()) {
        return;
    }

    const TakServerConfig& config = m_servers[m_currentIndex];
    m_nameEdit->setText(config.name);
    m_addressEdit->setText(config.address);
    m_portSpin->setValue(config.port);
    m_certPathEdit->setText(config.certFilePath);
    m_certPasswordEdit->setText(config.certPassword);
    m_callsignEdit->setText(config.callsign);

    int colorIdx = m_colorCombo->findText(config.color);
    if (colorIdx >= 0) m_colorCombo->setCurrentIndex(colorIdx);

    int roleIdx = m_roleCombo->findText(config.role);
    if (roleIdx >= 0) m_roleCombo->setCurrentIndex(roleIdx);

    m_autoConnectCheck->setChecked(config.autoConnect);
    m_debugLoggingCheck->setChecked(config.debugLogging);
}

void TakSettingsDialog::saveFormToCurrentServer()
{
    if (m_currentIndex < 0 || m_currentIndex >= m_servers.size()) {
        return;
    }

    TakServerConfig& config = m_servers[m_currentIndex];
    config.name = m_nameEdit->text().trimmed();
    config.address = m_addressEdit->text().trimmed();
    config.port = m_portSpin->value();
    config.certFilePath = m_certPathEdit->text().trimmed();
    config.certPassword = m_certPasswordEdit->text();
    config.callsign = m_callsignEdit->text().trimmed();
    config.color = m_colorCombo->currentText();
    config.role = m_roleCombo->currentText();
    config.autoConnect = m_autoConnectCheck->isChecked();
    config.debugLogging = m_debugLoggingCheck->isChecked();

    if (config.name.isEmpty()) {
        config.name = config.address.isEmpty() ? "Unnamed Server" : config.address;
    }

    updateServerListDisplay();
}

void TakSettingsDialog::updateServerListDisplay()
{
    m_serverList->clear();
    for (const TakServerConfig& config : m_servers) {
        QString display = config.name;
        if (!config.address.isEmpty()) {
            display += QString(" (%1:%2)").arg(config.address).arg(config.port);
        }
        m_serverList->addItem(display);
    }
}

QList<TakServerConfig> TakSettingsDialog::serverConfigs() const
{
    QList<TakServerConfig> result = m_servers;
    qDebug() << "TAK Dialog: serverConfigs() - m_currentIndex=" << m_currentIndex << "servers=" << result.size();
    if (m_currentIndex >= 0 && m_currentIndex < result.size()) {
        TakServerConfig copy = result[m_currentIndex];
        copy.name = m_nameEdit->text().trimmed();
        copy.address = m_addressEdit->text().trimmed();
        copy.port = m_portSpin->value();
        copy.certFilePath = m_certPathEdit->text().trimmed();
        copy.certPassword = m_certPasswordEdit->text();
        copy.callsign = m_callsignEdit->text().trimmed();
        copy.color = m_colorCombo->currentText();
        copy.role = m_roleCombo->currentText();
        copy.autoConnect = m_autoConnectCheck->isChecked();
        copy.debugLogging = m_debugLoggingCheck->isChecked();
        if (copy.name.isEmpty()) {
            copy.name = copy.address.isEmpty() ? "Unnamed Server" : copy.address;
        }
        qDebug() << "TAK Dialog: Form data - name:" << copy.name << "address:" << copy.address << "callsign:" << copy.callsign;
        result[m_currentIndex] = copy;
    } else {
        qWarning() << "TAK Dialog: m_currentIndex out of range, returning unmodified servers";
    }
    for (int i = 0; i < result.size(); ++i) {
        qDebug() << "TAK Dialog: Server" << i << "- id:" << result[i].id << "name:" << result[i].name << "address:" << result[i].address << "callsign:" << result[i].callsign;
    }
    return result;
}

void TakSettingsDialog::setServerConfigs(const QList<TakServerConfig>& configs)
{
    m_servers = configs;
    m_currentIndex = -1;
    updateServerListDisplay();

    if (!m_servers.isEmpty()) {
        QSignalBlocker blocker(m_serverList);
        m_serverList->setCurrentRow(0);
        m_currentIndex = 0;
        loadCurrentServerToForm();
        m_removeBtn->setEnabled(true);
    }
}
