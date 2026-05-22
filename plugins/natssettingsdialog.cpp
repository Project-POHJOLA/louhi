#include "natssettingsdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QGroupBox>
#include <QDateTime>

NatsSettingsDialog::NatsSettingsDialog(QWidget* parent)
    : QDialog(parent)
    , m_currentIndex(-1)
{
    setWindowTitle("NATS Communication Settings");
    setMinimumSize(600, 400);

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
    m_nameEdit->setPlaceholderText("My NATS Server");
    formLayout->addRow("Name:", m_nameEdit);

    m_urlEdit = new QLineEdit(configGroup);
    m_urlEdit->setPlaceholderText("localhost or nats.example.com");
    formLayout->addRow("Server URL:", m_urlEdit);

    m_portSpin = new QSpinBox(configGroup);
    m_portSpin->setRange(1, 65535);
    m_portSpin->setValue(4222);
    formLayout->addRow("Port:", m_portSpin);

    configLayout->addLayout(formLayout);

    m_autoConnectCheck = new QCheckBox("Auto-connect on startup", configGroup);
    configLayout->addWidget(m_autoConnectCheck);

    configLayout->addStretch();

    topLayout->addWidget(configGroup, 2);
    mainLayout->addLayout(topLayout);

    connect(m_serverList, &QListWidget::currentRowChanged, this, &NatsSettingsDialog::onServerSelected);
    connect(m_addBtn, &QPushButton::clicked, this, &NatsSettingsDialog::addServer);
    connect(m_removeBtn, &QPushButton::clicked, this, &NatsSettingsDialog::removeServer);

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

NatsSettingsDialog::~NatsSettingsDialog()
{
}

void NatsSettingsDialog::onServerSelected()
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

void NatsSettingsDialog::addServer()
{
    if (m_currentIndex >= 0 && m_currentIndex < m_servers.size()) {
        saveFormToCurrentServer();
    }

    NatsServerConfig config;
    config.id = QString("nats_server_%1").arg(QDateTime::currentMSecsSinceEpoch());
    config.name = "New Server";
    config.serverUrl = "localhost";
    config.port = 4222;
    config.autoConnect = false;

    m_servers.append(config);
    m_currentIndex = m_servers.size() - 1;

    updateServerListDisplay();
    m_serverList->setCurrentRow(m_currentIndex);
    loadCurrentServerToForm();
}

void NatsSettingsDialog::removeServer()
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

void NatsSettingsDialog::loadCurrentServerToForm()
{
    if (m_currentIndex < 0 || m_currentIndex >= m_servers.size()) {
        return;
    }

    const NatsServerConfig& config = m_servers[m_currentIndex];
    m_nameEdit->setText(config.name);
    m_urlEdit->setText(config.serverUrl);
    m_portSpin->setValue(config.port);
    m_autoConnectCheck->setChecked(config.autoConnect);
}

void NatsSettingsDialog::saveFormToCurrentServer()
{
    if (m_currentIndex < 0 || m_currentIndex >= m_servers.size()) {
        return;
    }

    NatsServerConfig& config = m_servers[m_currentIndex];
    config.name = m_nameEdit->text().trimmed();
    config.serverUrl = m_urlEdit->text().trimmed();
    config.port = m_portSpin->value();
    config.autoConnect = m_autoConnectCheck->isChecked();

    if (config.name.isEmpty()) {
        config.name = config.serverUrl.isEmpty() ? "Unnamed Server" : config.serverUrl;
    }

    updateServerListDisplay();
}

void NatsSettingsDialog::updateServerListDisplay()
{
    m_serverList->clear();
    for (const NatsServerConfig& config : m_servers) {
        QString display = config.name;
        if (!config.serverUrl.isEmpty()) {
            display += QString(" (%1:%2)").arg(config.serverUrl).arg(config.port);
        }
        m_serverList->addItem(display);
    }
}

QList<NatsServerConfig> NatsSettingsDialog::serverConfigs() const
{
    QList<NatsServerConfig> result = m_servers;
    if (m_currentIndex >= 0 && m_currentIndex < result.size()) {
        NatsServerConfig copy = result[m_currentIndex];
        copy.name = m_nameEdit->text().trimmed();
        copy.serverUrl = m_urlEdit->text().trimmed();
        copy.port = m_portSpin->value();
        copy.autoConnect = m_autoConnectCheck->isChecked();
        if (copy.name.isEmpty()) {
            copy.name = copy.serverUrl.isEmpty() ? "Unnamed Server" : copy.serverUrl;
        }
        result[m_currentIndex] = copy;
    }
    return result;
}

void NatsSettingsDialog::setServerConfigs(const QList<NatsServerConfig>& configs)
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
