#include "natssettingsdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QGroupBox>
#include <QPushButton>

NatsSettingsDialog::NatsSettingsDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("NATS Communication Settings");
    setMinimumSize(400, 200);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    QGroupBox* serverGroup = new QGroupBox("Server Connection", this);
    QFormLayout* serverLayout = new QFormLayout(serverGroup);

    m_serverUrlEdit = new QLineEdit("localhost", this);
    m_serverUrlEdit->setPlaceholderText("localhost or nats.example.com");
    serverLayout->addRow("Server URL:", m_serverUrlEdit);

    m_portSpin = new QSpinBox(this);
    m_portSpin->setRange(1, 65535);
    m_portSpin->setValue(4222);
    serverLayout->addRow("Port:", m_portSpin);

    mainLayout->addWidget(serverGroup);

    m_autoConnectCheck = new QCheckBox("Auto-connect on startup", this);
    mainLayout->addWidget(m_autoConnectCheck);

    mainLayout->addStretch();

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

QString NatsSettingsDialog::serverUrl() const
{
    return m_serverUrlEdit->text();
}

int NatsSettingsDialog::port() const
{
    return m_portSpin->value();
}

bool NatsSettingsDialog::autoConnect() const
{
    return m_autoConnectCheck->isChecked();
}

void NatsSettingsDialog::setServerUrl(const QString& url)
{
    m_serverUrlEdit->setText(url);
}

void NatsSettingsDialog::setPort(int port)
{
    m_portSpin->setValue(port);
}

void NatsSettingsDialog::setAutoConnect(bool autoConnect)
{
    m_autoConnectCheck->setChecked(autoConnect);
}
