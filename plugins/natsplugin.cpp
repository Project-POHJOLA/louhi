#include "natsplugin.h"
#include "natsclient.h"
#include "natssettingsdialog.h"

#include <QLabel>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QDebug>

NatsPlugin::NatsPlugin(QObject* parent)
    : PluginInterface(parent)
    , m_natsClient(new NatsClient(this))
    , m_statusWidget(nullptr)
    , m_port(4222)
    , m_autoConnect(false)
{
    m_info.id = "nats_communication";
    m_info.name = "NATS Communication";
    m_info.version = "0.1";
    m_info.description = "NATS transport plugin - subscribes to topics requested by other plugins";
    m_info.author = "LOUHI Team";
    m_info.type = PluginType::Communication;
    m_info.enabled = true;
    m_info.dependencies = QStringList();
    m_info.capabilities = QStringList() << "Publish" << "Subscribe" << "Request-Reply";
    m_info.subscribeTopics = QStringList();
    m_info.publishTopics = QStringList() << "*";

    m_serverUrl = "localhost";

    connect(m_natsClient, &NatsClient::connected, this, [this]() {
        emit statusChanged("Connected to " + m_serverUrl);
        emit connectionStatusChanged("NATS", m_serverUrl + ":Connected");
    });
    connect(m_natsClient, &NatsClient::disconnected, this, [this]() {
        emit statusChanged("Disconnected");
        emit connectionStatusChanged("NATS", m_serverUrl + ":Disconnected");
    });
    connect(m_natsClient, &NatsClient::messageReceived,
            this, &NatsPlugin::messageReceived);

    connect(this, &PluginInterface::statusChanged, this, [this](const QString& status) {
        if (status == "connect") {
            QString url = QString("%1:%2").arg(m_serverUrl).arg(m_port);
            m_natsClient->connectToServer(url);
            for (const QString& topic : m_subscribedTopics) {
                m_natsClient->subscribe(topic);
            }
        } else if (status == "disconnect") {
            m_natsClient->disconnect();
        }
    });
}

NatsPlugin::~NatsPlugin()
{
    unload();
}

PluginInfo NatsPlugin::getPluginInfo() const
{
    return m_info;
}

QVector<MenuEntry> NatsPlugin::getMenuEntries() const
{
    QVector<MenuEntry> entries;
    MenuEntry commEntry;
    commEntry.topMenu = "Communication";
    commEntry.subMenus = QStringList() << "Connect" << "Disconnect";
    entries.append(commEntry);

    MenuEntry settingsEntry;
    settingsEntry.topMenu = "Settings";
    settingsEntry.subMenus = QStringList();
    entries.append(settingsEntry);

    return entries;
}

bool NatsPlugin::load()
{
    qDebug() << "NATS Plugin: Loading";
    return true;
}

bool NatsPlugin::initialize()
{
    qDebug() << "NATS Plugin: Initializing, will subscribe to:" << m_subscribedTopics;
    m_statusWidget = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(m_statusWidget);

    QGroupBox* statusGroup = new QGroupBox("Connection Status", m_statusWidget);
    QVBoxLayout* statusLayout = new QVBoxLayout(statusGroup);

    QLabel* serverLabel = new QLabel(QString("Server: %1:%2").arg(m_serverUrl).arg(m_port), m_statusWidget);
    QLabel* statusLabel = new QLabel("Status: Not connected", m_statusWidget);
    QLabel* topicsLabel = new QLabel(QString("Subscribed: %1").arg(m_subscribedTopics.isEmpty() ? "(none)" : m_subscribedTopics.join(", ")), m_statusWidget);

    statusLayout->addWidget(serverLabel);
    statusLayout->addWidget(statusLabel);
    statusLayout->addWidget(topicsLabel);

    QPushButton* connectBtn = new QPushButton("Connect", m_statusWidget);
    QPushButton* disconnectBtn = new QPushButton("Disconnect", m_statusWidget);

    connect(connectBtn, &QPushButton::clicked, this, [this]() {
        QString url = QString("%1:%2").arg(m_serverUrl).arg(m_port);
        m_natsClient->connectToServer(url);
        for (const QString& topic : m_subscribedTopics) {
            m_natsClient->subscribe(topic);
        }
    });

    connect(disconnectBtn, &QPushButton::clicked, this, [this]() {
        m_natsClient->disconnect();
    });

    statusLayout->addWidget(connectBtn);
    statusLayout->addWidget(disconnectBtn);

    layout->addWidget(statusGroup);
    layout->addStretch();

    emit connectionStatusChanged("NATS", m_serverUrl + ":Disconnected");

    if (m_autoConnect) {
        QString url = QString("%1:%2").arg(m_serverUrl).arg(m_port);
        m_natsClient->connectToServer(url);
        for (const QString& topic : m_subscribedTopics) {
            m_natsClient->subscribe(topic);
        }
    }

    return true;
}

bool NatsPlugin::start()
{
    qDebug() << "NATS Plugin: Starting";
    return true;
}

bool NatsPlugin::stop()
{
    qDebug() << "NATS Plugin: Stopping";
    return true;
}

bool NatsPlugin::unload()
{
    qDebug() << "NATS Plugin: Unloading";
    m_natsClient->disconnect();
    delete m_statusWidget;
    m_statusWidget = nullptr;
    return true;
}

QWidget* NatsPlugin::getWidget()
{
    return m_statusWidget;
}

void NatsPlugin::configure(QWidget* parent)
{
    NatsSettingsDialog dialog(parent);
    dialog.setServerUrl(m_serverUrl);
    dialog.setPort(m_port);
    dialog.setAutoConnect(m_autoConnect);

    if (dialog.exec() == QDialog::Accepted) {
        m_serverUrl = dialog.serverUrl();
        m_port = dialog.port();
        m_autoConnect = dialog.autoConnect();

        if (m_natsClient->isConnected()) {
            m_natsClient->disconnect();
            QString url = QString("%1:%2").arg(m_serverUrl).arg(m_port);
            m_natsClient->connectToServer(url);
            for (const QString& topic : m_subscribedTopics) {
                m_natsClient->subscribe(topic);
            }
        }

        updateStatusWidget();
    }
}

QJsonObject NatsPlugin::getConfig() const
{
    QJsonObject config;
    config["serverUrl"] = m_serverUrl;
    config["port"] = m_port;
    config["autoConnect"] = m_autoConnect;
    return config;
}

void NatsPlugin::setConfig(const QJsonObject& config)
{
    m_serverUrl = config.value("serverUrl").toString("localhost");
    m_port = config.value("port").toInt(4222);
    m_autoConnect = config.value("autoConnect").toBool(false);
}

void NatsPlugin::setSubscribedTopics(const QStringList& topics)
{
    m_subscribedTopics = topics;
    qDebug() << "NATS Plugin: Subscribed topics set to:" << topics;
}

void NatsPlugin::updateStatusWidget()
{
}
