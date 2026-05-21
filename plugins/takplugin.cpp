#include "takplugin.h"
#include "taksettingsdialog.h"
#include "cotmessage.h"
#include "version.h"

#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QDebug>
#include <QListWidget>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTimer>
#include <QSysInfo>

TakPlugin::TakPlugin(QObject* parent)
    : PluginInterface(parent)
    , m_statusWidget(nullptr)
    , m_mainLayout(nullptr)
    , m_serverStatusList(nullptr)
    , m_connectionLabel(nullptr)
    , m_deviceUid(QUuid::createUuid().toString(QUuid::WithoutBraces).left(8).toUpper())
{
    m_info.id = "tak_communication";
    m_info.name = "TAK Communication";
    m_info.version = "0.1";
    m_info.description = "Team Awareness Kit (TAK) server communication via CoT over TCP/TLS";
    m_info.author = "LOUHI Team";
    m_info.type = PluginType::Communication;
    m_info.enabled = true;
    m_info.dependencies = QStringList();
    m_info.capabilities = QStringList() << "CoT" << "TCP/TLS" << "Multi-Server" << "Position Reports" << "Chat";
    m_info.subscribeTopics = QStringList() << "tak.>" << "location.position";
    m_info.publishTopics = QStringList() << "tak.>";

    connect(this, &PluginInterface::statusChanged, this, [this](const QString& status) {
        if (status == "connect") {
            for (auto it = m_connections.begin(); it != m_connections.end(); ++it) {
                it.value()->connectToServer();
            }
        } else if (status == "disconnect") {
            for (auto it = m_connections.begin(); it != m_connections.end(); ++it) {
                it.value()->disconnect();
            }
        }
    });
}

TakPlugin::~TakPlugin()
{
    unload();
}

PluginInfo TakPlugin::getPluginInfo() const
{
    return m_info;
}

QVector<MenuEntry> TakPlugin::getMenuEntries() const
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

bool TakPlugin::load()
{
    qDebug() << "TAK Plugin: Loading";
    return true;
}

bool TakPlugin::initialize()
{
    qDebug() << "TAK Plugin: Initializing with" << m_serverConfigs.size() << "server configs";

    buildStatusWidget();

    for (const TakServerConfig& config : m_serverConfigs) {
        qDebug() << "TAK Plugin: Creating connection for" << config.name << "at" << config.address << ":" << config.port << "autoConnect:" << config.autoConnect;
        TakServerConnection* conn = new TakServerConnection(config, this);

        connect(conn, &TakServerConnection::connected, this, &TakPlugin::onServerConnected);
        connect(conn, &TakServerConnection::disconnected, this, &TakPlugin::onServerDisconnected);
        connect(conn, &TakServerConnection::messageReceived, this, &TakPlugin::onServerMessageReceived);
        connect(conn, &TakServerConnection::connectionError, this, &TakPlugin::onServerError);
        connect(conn, &TakServerConnection::statusChanged, this, &TakPlugin::onServerStatusChanged);

        m_connections[config.id] = conn;

        emit connectionStatusChanged("TAK", config.name + ":Disconnected");

        if (config.autoConnect) {
            qDebug() << "TAK Plugin: Auto-connecting to" << config.name;
            conn->connectToServer();
        } else {
            qDebug() << "TAK Plugin: Not auto-connecting to" << config.name << "(autoConnect disabled)";
        }
    }

    updateStatusDisplay();
    return true;
}

bool TakPlugin::start()
{
    qDebug() << "TAK Plugin: Starting";
    return true;
}

bool TakPlugin::stop()
{
    qDebug() << "TAK Plugin: Stopping";
    for (auto it = m_connections.begin(); it != m_connections.end(); ++it) {
        it.value()->disconnect();
    }
    return true;
}

bool TakPlugin::unload()
{
    qDebug() << "TAK Plugin: Unloading";

    for (auto it = m_connections.begin(); it != m_connections.end(); ++it) {
        it.value()->disconnect();
        it.value()->deleteLater();
    }
    m_connections.clear();

    delete m_statusWidget;
    m_statusWidget = nullptr;

    return true;
}

QWidget* TakPlugin::getWidget()
{
    return m_statusWidget;
}

void TakPlugin::configure(QWidget* parent)
{
    TakSettingsDialog dialog(parent);
    dialog.setServerConfigs(m_serverConfigs);

    if (dialog.exec() == QDialog::Accepted) {
        QList<TakServerConfig> newConfigs = dialog.serverConfigs();
        qDebug() << "TAK Plugin: Received" << newConfigs.size() << "server configs from dialog";
        for (const TakServerConfig& c : newConfigs) {
            qDebug() << "TAK Plugin: Server - id:" << c.id << "name:" << c.name << "address:" << c.address << "callsign:" << c.callsign;
        }

        QSet<QString> oldIds;
        for (const TakServerConfig& config : m_serverConfigs) {
            oldIds.insert(config.id);
        }

        QSet<QString> newIds;
        for (const TakServerConfig& config : newConfigs) {
            newIds.insert(config.id);
        }

        for (const QString& id : oldIds) {
            if (!newIds.contains(id)) {
                if (m_connections.contains(id)) {
                    m_connections[id]->disconnect();
                    m_connections[id]->deleteLater();
                    m_connections.remove(id);
                }
            }
        }

        for (const TakServerConfig& config : newConfigs) {
            if (m_connections.contains(config.id)) {
                m_connections[config.id]->updateConfig(config);
            } else {
                TakServerConnection* conn = new TakServerConnection(config, this);

                connect(conn, &TakServerConnection::connected, this, &TakPlugin::onServerConnected);
                connect(conn, &TakServerConnection::disconnected, this, &TakPlugin::onServerDisconnected);
                connect(conn, &TakServerConnection::messageReceived, this, &TakPlugin::onServerMessageReceived);
                connect(conn, &TakServerConnection::connectionError, this, &TakPlugin::onServerError);
                connect(conn, &TakServerConnection::statusChanged, this, &TakPlugin::onServerStatusChanged);

                m_connections[config.id] = conn;

                if (config.autoConnect) {
                    conn->connectToServer();
                }
            }
        }

        m_serverConfigs = newConfigs;
        updateStatusDisplay();
    }
}

QJsonObject TakPlugin::getConfig() const
{
    QJsonObject config;
    QJsonArray serversArray;

    for (const TakServerConfig& serverConfig : m_serverConfigs) {
        serversArray.append(configToJson(serverConfig));
    }

    config["servers"] = serversArray;
    qDebug() << "TAK Plugin: Saving config with" << serversArray.size() << "servers";
    return config;
}

void TakPlugin::setConfig(const QJsonObject& config)
{
    m_serverConfigs.clear();

    QJsonArray serversArray = config.value("servers").toArray();
    qDebug() << "TAK Plugin: Loading config with" << serversArray.size() << "servers";
    for (const QJsonValue& v : serversArray) {
        TakServerConfig cfg = configFromJson(v.toObject());
        qDebug() << "TAK Plugin: Loaded server - id:" << cfg.id << "name:" << cfg.name << "address:" << cfg.address << "callsign:" << cfg.callsign;
        m_serverConfigs.append(cfg);
    }
}

void TakPlugin::onServerConnected()
{
    TakServerConnection* conn = qobject_cast<TakServerConnection*>(sender());
    if (conn) {
        emit connectionStatusChanged("TAK", conn->config().name + ":Connected");
    }
    updateStatusDisplay();
}

void TakPlugin::onServerDisconnected()
{
    TakServerConnection* conn = qobject_cast<TakServerConnection*>(sender());
    if (conn) {
        emit connectionStatusChanged("TAK", conn->config().name + ":Disconnected");
    }
    updateStatusDisplay();
}

void TakPlugin::onServerMessageReceived(const QString& xml)
{
    TakServerConnection* conn = qobject_cast<TakServerConnection*>(sender());
    if (!conn) return;

    QString serverId = conn->config().id;
    publishToNats(serverId, xml);

    CoTMessage msg = CoTMessageParser::parse(xml);
    if (conn->config().debugLogging) {
        qDebug() << "TAK:" << msg.contact.callsign << "uid:" << msg.uid
                 << "lat:" << msg.point.lat << "lon:" << msg.point.lon;
    }
}

void TakPlugin::onServerError(const QString& error)
{
    qDebug() << "TAK Error:" << error;
    updateStatusDisplay();
}

void TakPlugin::onServerStatusChanged(const QString& status)
{
    Q_UNUSED(status);
    updateStatusDisplay();
}

void TakPlugin::deliverMessage(const QString& topic, const QString& payload)
{
    PluginInterface::deliverMessage(topic, payload);

    if (topic == "location.position") {
        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(payload.toUtf8(), &error);
        if (error.error != QJsonParseError::NoError) {
            qDebug() << "TAK Plugin: Failed to parse location JSON:" << error.errorString();
            return;
        }

        QJsonObject loc = doc.object();
        double lat = loc.value("latitude").toDouble();
        double lon = loc.value("longitude").toDouble();
        double alt = loc.value("altitude").toDouble(0.0);
        double hdop = loc.value("hdop").toDouble(99.9);
        QString source = loc.value("source").toString();

        QString how = "m-g";
        QString sourceLower = source.toLower();
        if (sourceLower == "manual") {
            how = "h-e";
        }

        double ce = (sourceLower == "manual") ? 1.0 : hdop;
        double le = 999999.9;

        for (const TakServerConfig& cfg : m_serverConfigs) {
            if (m_connections.contains(cfg.id)) {
                TakServerConnection* conn = m_connections[cfg.id];
                if (conn->isConnected()) {
                    QString uid = QString("LOUHI-%1").arg(cfg.callsign);
                    QString cotType = cfg.cotType.isEmpty() ? "a-f-G-U" : cfg.cotType;
                    QString cotXml = CoTMessageBuilder::buildPositionReport(
                        uid,
                        cfg.callsign,
                        cotType,
                        how,
                        lat,
                        lon,
                        alt,
                        ce,
                        le,
                        cfg.color,
                        cfg.role,
                        m_deviceUid,
                        QSysInfo::prettyProductName(),
                        "LOUHI",
                        LOUHI_VERSION_STRING
                    );
                    conn->sendCoT(cotXml);

                    if (cfg.debugLogging) {
                        qDebug() << "TAK Plugin: Sent CoT for" << cfg.callsign
                                 << "lat:" << lat << "lon:" << lon
                                 << "how:" << how << "source:" << source
                                 << "type:" << cotType
                                 << "ce:" << ce << "le:" << le;
                    }
                }
            }
        }
    }
}

void TakPlugin::sendCoTToAllServers(const QString& xml)
{
    for (const TakServerConfig& config : m_serverConfigs) {
        if (m_connections.contains(config.id)) {
            TakServerConnection* conn = m_connections[config.id];
            if (conn->isConnected()) {
                conn->sendCoT(xml);
            }
        }
    }
}

void TakPlugin::buildStatusWidget()
{
    m_statusWidget = new QWidget();
    m_mainLayout = new QVBoxLayout(m_statusWidget);

    QGroupBox* statusGroup = new QGroupBox("TAK Server Connections", m_statusWidget);
    QVBoxLayout* groupLayout = new QVBoxLayout(statusGroup);

    m_serverStatusList = new QListWidget(statusGroup);
    m_serverStatusList->setSelectionMode(QAbstractItemView::NoSelection);

    QHBoxLayout* btnLayout = new QHBoxLayout();
    QPushButton* connectAllBtn = new QPushButton("Connect All", statusGroup);
    QPushButton* disconnectAllBtn = new QPushButton("Disconnect All", statusGroup);
    QPushButton* configureBtn = new QPushButton("Configure...", statusGroup);
    btnLayout->addWidget(connectAllBtn);
    btnLayout->addWidget(disconnectAllBtn);
    btnLayout->addStretch();
    btnLayout->addWidget(configureBtn);

    groupLayout->addWidget(m_serverStatusList);
    groupLayout->addLayout(btnLayout);

    m_mainLayout->addWidget(statusGroup);
    m_mainLayout->addStretch();

    connect(connectAllBtn, &QPushButton::clicked, this, [this]() {
        for (const TakServerConfig& config : m_serverConfigs) {
            if (m_connections.contains(config.id)) {
                m_connections[config.id]->connectToServer();
            }
        }
    });

    connect(disconnectAllBtn, &QPushButton::clicked, this, [this]() {
        for (auto it = m_connections.begin(); it != m_connections.end(); ++it) {
            it.value()->disconnect();
        }
    });

    connect(configureBtn, &QPushButton::clicked, this, [this]() {
        configure(m_statusWidget);
    });
}

void TakPlugin::updateStatusDisplay()
{
    if (!m_serverStatusList) return;

    m_serverStatusList->clear();

    for (const TakServerConfig& config : m_serverConfigs) {
        QString statusText;
        QString icon;

        if (m_connections.contains(config.id)) {
            TakServerConnection* conn = m_connections[config.id];
            if (conn->isConnected()) {
                icon = "[+]";
                statusText = QString("%1 %2 - %3")
                    .arg(icon)
                    .arg(config.name)
                    .arg(conn->statusText());
            } else {
                icon = "[-]";
                statusText = QString("%1 %2 - %3")
                    .arg(icon)
                    .arg(config.name)
                    .arg(conn->statusText());
            }
        } else {
            icon = "[ ]";
            statusText = QString("%1 %2 - Not initialized")
                .arg(icon)
                .arg(config.name);
        }

        m_serverStatusList->addItem(statusText);
    }
}

void TakPlugin::connectToServer(const QString& serverId)
{
    if (m_connections.contains(serverId)) {
        m_connections[serverId]->connectToServer();
    }
}

void TakPlugin::disconnectFromServer(const QString& serverId)
{
    if (m_connections.contains(serverId)) {
        m_connections[serverId]->disconnect();
    }
}

void TakPlugin::publishToNats(const QString& serverId, const QString& xml)
{
    QString topic = QString("tak.%1").arg(sanitizeTopicName(serverId));
    emit messageReceived(topic, xml);
}

QString TakPlugin::sanitizeTopicName(const QString& input) const
{
    QString result = input;
    result.replace(' ', '_');
    result.replace('.', '_');
    result.replace('/', '_');
    result.replace('>', '_');
    result.replace('+', '_');
    result.replace('*', '_');
    return result;
}

TakServerConfig TakPlugin::configFromJson(const QJsonObject& obj) const
{
    TakServerConfig config;
    config.id = obj.value("id").toString("");
    config.name = obj.value("name").toString("Unnamed");
    config.address = obj.value("address").toString("");
    config.port = obj.value("port").toInt(8089);
    config.certFilePath = obj.value("certFilePath").toString("");
    config.certPassword = obj.value("certPassword").toString("");
    config.callsign = obj.value("callsign").toString("Unknown");
    config.color = obj.value("color").toString("Unknown");
    config.role = obj.value("role").toString("Team Member");
    config.cotType = obj.value("cotType").toString("a-f-G-U");
    config.autoConnect = obj.value("autoConnect").toBool(false);
    config.debugLogging = obj.value("debugLogging").toBool(false);
    return config;
}

QJsonObject TakPlugin::configToJson(const TakServerConfig& config) const
{
    QJsonObject obj;
    obj["id"] = config.id;
    obj["name"] = config.name;
    obj["address"] = config.address;
    obj["port"] = config.port;
    obj["certFilePath"] = config.certFilePath;
    obj["certPassword"] = config.certPassword;
    obj["callsign"] = config.callsign;
    obj["color"] = config.color;
    obj["role"] = config.role;
    obj["cotType"] = config.cotType;
    obj["autoConnect"] = config.autoConnect;
    obj["debugLogging"] = config.debugLogging;
    return obj;
}
