#include "natsplugin.h"
#include "natsclient.h"
#include "natssettingsdialog.h"

#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QDebug>
#include <QListWidget>
#include <QJsonArray>
#include <QJsonDocument>

NatsPlugin::NatsPlugin(QObject* parent)
    : PluginInterface(parent)
    , m_statusWidget(nullptr)
    , m_mainLayout(nullptr)
    , m_serverStatusList(nullptr)
{
    m_info.id = "nats_communication";
    m_info.name = tr("NATS Communication");
    m_info.version = "0.1";
    m_info.description = tr("NATS transport plugin - subscribes to topics requested by other plugins");
    m_info.author = "LOUHI Team";
    m_info.type = PluginType::Communication;
    m_info.enabled = true;
    m_info.dependencies = QStringList();
    m_info.capabilities = QStringList() << "Publish" << "Subscribe" << "Request-Reply" << "Multi-Server";
    m_info.subscribeTopics = QStringList();
    m_info.publishTopics = QStringList() << "*";

    connect(this, &PluginInterface::statusChanged, this, [this](const QString& status) {
        if (status == "connect") {
            for (const NatsServerConfig& config : m_serverConfigs) {
                if (m_clients.contains(config.id)) {
                    NatsClient* client = m_clients[config.id];
                    QString url = QString("%1:%2").arg(config.serverUrl).arg(config.port);
                    client->connectToServer(url);
                    subscribeAllTopics(client);
                }
            }
        } else if (status == "disconnect") {
            for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
                it.value()->disconnect();
            }
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
    commEntry.topMenu = tr("Communication");
    commEntry.subMenus = QStringList() << tr("Connect") << tr("Disconnect");
    entries.append(commEntry);

    MenuEntry settingsEntry;
    settingsEntry.topMenu = tr("Settings");
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
    qDebug() << "NATS Plugin: Initializing with" << m_serverConfigs.size() << "server configs";

    buildStatusWidget();

    for (const NatsServerConfig& config : m_serverConfigs) {
        qDebug() << "NATS Plugin: Creating client for" << config.name << "at" << config.serverUrl << ":" << config.port << "autoConnect:" << config.autoConnect;
        NatsClient* client = new NatsClient(this);

        connect(client, &NatsClient::connected, this, &NatsPlugin::onClientConnected);
        connect(client, &NatsClient::disconnected, this, &NatsPlugin::onClientDisconnected);
        connect(client, &NatsClient::messageReceived, this, &NatsPlugin::messageReceived);

        m_clients[config.id] = client;

        emit connectionStatusChanged("NATS", config.name + ":Disconnected");

        if (config.autoConnect) {
            QString url = QString("%1:%2").arg(config.serverUrl).arg(config.port);
            client->connectToServer(url);
            subscribeAllTopics(client);
        }
    }

    updateStatusDisplay();
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
    for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
        it.value()->disconnect();
    }
    return true;
}

bool NatsPlugin::unload()
{
    qDebug() << "NATS Plugin: Unloading";

    for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
        it.value()->disconnect();
        it.value()->deleteLater();
    }
    m_clients.clear();

    delete m_statusWidget;
    m_statusWidget = nullptr;

    return true;
}

void NatsPlugin::publish(const QString& topic, const QString& payload)
{
    if (m_emconActive) {
        qDebug() << "NATS Plugin: EMCON active, dropping outbound message on" << topic;
        return;
    }

    if (m_clients.isEmpty()) {
        return;
    }

    for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
        if (it.value()->isConnected()) {
            it.value()->publish(topic, payload);
        }
    }
}

QWidget* NatsPlugin::getWidget()
{
    return m_statusWidget;
}

void NatsPlugin::configure(QWidget* parent)
{
    NatsSettingsDialog dialog(parent);
    dialog.setServerConfigs(m_serverConfigs);
    dialog.setEmconEnabled(m_emconActive);

    if (dialog.exec() == QDialog::Accepted) {
        QList<NatsServerConfig> newConfigs = dialog.serverConfigs();
        qDebug() << "NATS Plugin: Received" << newConfigs.size() << "server configs from dialog";

        QSet<QString> oldIds;
        for (const NatsServerConfig& config : m_serverConfigs) {
            oldIds.insert(config.id);
        }

        QSet<QString> newIds;
        for (const NatsServerConfig& config : newConfigs) {
            newIds.insert(config.id);
        }

        for (const QString& id : oldIds) {
            if (!newIds.contains(id)) {
                if (m_clients.contains(id)) {
                    m_clients[id]->disconnect();
                    m_clients[id]->deleteLater();
                    m_clients.remove(id);
                }
            }
        }

        for (const NatsServerConfig& config : newConfigs) {
            if (m_clients.contains(config.id)) {
            } else {
                NatsClient* client = new NatsClient(this);
                connect(client, &NatsClient::connected, this, &NatsPlugin::onClientConnected);
                connect(client, &NatsClient::disconnected, this, &NatsPlugin::onClientDisconnected);
                connect(client, &NatsClient::messageReceived, this, &NatsPlugin::messageReceived);
                m_clients[config.id] = client;

                if (config.autoConnect) {
                    QString url = QString("%1:%2").arg(config.serverUrl).arg(config.port);
                    client->connectToServer(url);
                    subscribeAllTopics(client);
                }
            }
        }

        m_serverConfigs = newConfigs;

        bool newEmcon = dialog.emconEnabled();
        if (newEmcon != m_emconActive) {
            m_emconActive = newEmcon;
            emit emconStateChanged(m_emconActive);
        }

        updateStatusDisplay();
    }
}

QJsonObject NatsPlugin::getConfig() const
{
    QJsonObject config;
    QJsonArray serversArray;

    for (const NatsServerConfig& serverConfig : m_serverConfigs) {
        serversArray.append(configToJson(serverConfig));
    }

    config["servers"] = serversArray;
    config["emcon"] = m_emconActive;
    return config;
}

void NatsPlugin::setConfig(const QJsonObject& config)
{
    m_serverConfigs.clear();

    QJsonArray serversArray = config.value("servers").toArray();
    for (const QJsonValue& v : serversArray) {
        NatsServerConfig cfg = configFromJson(v.toObject());
        m_serverConfigs.append(cfg);
    }

    m_emconActive = config.value("emcon").toBool(false);
}


void NatsPlugin::setSubscribedTopics(const QStringList& topics)
{
    m_subscribedTopics = topics;
    qDebug() << "NATS Plugin: Subscribed topics set to:" << topics;

    for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
        if (it.value()->isConnected()) {
            subscribeAllTopics(it.value());
        }
    }
}

void NatsPlugin::onClientConnected()
{
    NatsClient* client = qobject_cast<NatsClient*>(sender());
    if (client) {
        for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
            if (it.value() == client) {
                for (const NatsServerConfig& cfg : m_serverConfigs) {
                    if (cfg.id == it.key()) {
                        emit connectionStatusChanged("NATS", cfg.name + ":Connected");
                        break;
                    }
                }
                break;
            }
        }
    }
    updateStatusDisplay();
}

void NatsPlugin::onClientDisconnected()
{
    NatsClient* client = qobject_cast<NatsClient*>(sender());
    if (client) {
        for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
            if (it.value() == client) {
                for (const NatsServerConfig& cfg : m_serverConfigs) {
                    if (cfg.id == it.key()) {
                        emit connectionStatusChanged("NATS", cfg.name + ":Disconnected");
                        break;
                    }
                }
                break;
            }
        }
    }
    updateStatusDisplay();
}

void NatsPlugin::buildStatusWidget()
{
    m_statusWidget = new QWidget();
    m_mainLayout = new QVBoxLayout(m_statusWidget);

    QGroupBox* statusGroup = new QGroupBox(tr("NATS Server Connections"), m_statusWidget);
    QVBoxLayout* groupLayout = new QVBoxLayout(statusGroup);

    m_serverStatusList = new QListWidget(statusGroup);
    m_serverStatusList->setSelectionMode(QAbstractItemView::NoSelection);

    QHBoxLayout* btnLayout = new QHBoxLayout();
    QPushButton* connectAllBtn = new QPushButton(tr("Connect All"), statusGroup);
    QPushButton* disconnectAllBtn = new QPushButton(tr("Disconnect All"), statusGroup);
    QPushButton* configureBtn = new QPushButton(tr("Configure..."), statusGroup);
    btnLayout->addWidget(connectAllBtn);
    btnLayout->addWidget(disconnectAllBtn);
    btnLayout->addStretch();
    btnLayout->addWidget(configureBtn);

    groupLayout->addWidget(m_serverStatusList);
    groupLayout->addLayout(btnLayout);

    m_mainLayout->addWidget(statusGroup);
    m_mainLayout->addStretch();

    connect(connectAllBtn, &QPushButton::clicked, this, [this]() {
        for (const NatsServerConfig& config : m_serverConfigs) {
            if (m_clients.contains(config.id)) {
                NatsClient* client = m_clients[config.id];
                QString url = QString("%1:%2").arg(config.serverUrl).arg(config.port);
                client->connectToServer(url);
                subscribeAllTopics(client);
            }
        }
    });

    connect(disconnectAllBtn, &QPushButton::clicked, this, [this]() {
        for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
            it.value()->disconnect();
        }
    });

    connect(configureBtn, &QPushButton::clicked, this, [this]() {
        configure(m_statusWidget);
    });
}

void NatsPlugin::updateStatusDisplay()
{
    if (!m_serverStatusList) return;

    m_serverStatusList->clear();

    for (const NatsServerConfig& config : m_serverConfigs) {
        QString statusText;
        QString icon;

        if (m_clients.contains(config.id)) {
            NatsClient* client = m_clients[config.id];
            if (client->isConnected()) {
                icon = "[+]";
                statusText = QString("%1 %2 - " + tr("Connected to %3:%4"))
                    .arg(icon)
                    .arg(config.name)
                    .arg(config.serverUrl)
                    .arg(config.port);
            } else {
                icon = "[-]";
                statusText = QString("%1 %2 - " + tr("Disconnected"))
                    .arg(icon)
                    .arg(config.name);
            }
        } else {
            icon = "[ ]";
            statusText = QString("%1 %2 - " + tr("Not initialized"))
                .arg(icon)
                .arg(config.name);
        }

        m_serverStatusList->addItem(statusText);
    }
}

void NatsPlugin::subscribeAllTopics(NatsClient* client)
{
    for (const QString& topic : m_subscribedTopics) {
        client->subscribe(topic);
    }
}

void NatsPlugin::connectToServer(const QString& serverId)
{
    if (m_clients.contains(serverId)) {
        for (const NatsServerConfig& config : m_serverConfigs) {
            if (config.id == serverId) {
                NatsClient* client = m_clients[serverId];
                QString url = QString("%1:%2").arg(config.serverUrl).arg(config.port);
                client->connectToServer(url);
                subscribeAllTopics(client);
                break;
            }
        }
    }
}

void NatsPlugin::disconnectFromServer(const QString& serverId)
{
    if (m_clients.contains(serverId)) {
        m_clients[serverId]->disconnect();
    }
}

NatsServerConfig NatsPlugin::configFromJson(const QJsonObject& obj) const
{
    NatsServerConfig config;
    config.id = obj.value("id").toString("");
    config.name = obj.value("name").toString("Unnamed");
    config.serverUrl = obj.value("serverUrl").toString("localhost");
    config.port = obj.value("port").toInt(4222);
    config.autoConnect = obj.value("autoConnect").toBool(false);
    return config;
}

QJsonObject NatsPlugin::configToJson(const NatsServerConfig& config) const
{
    QJsonObject obj;
    obj["id"] = config.id;
    obj["name"] = config.name;
    obj["serverUrl"] = config.serverUrl;
    obj["port"] = config.port;
    obj["autoConnect"] = config.autoConnect;
    return obj;
}
