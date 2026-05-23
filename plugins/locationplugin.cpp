#include "locationplugin.h"
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QDebug>
#include <QJsonDocument>

LocationPlugin::LocationPlugin(QObject* parent)
    : PluginInterface(parent)
    , m_statusWidget(nullptr)
    , m_mainLayout(nullptr)
    , m_statusLabel(nullptr)
    , m_locationLabel(nullptr)
    , m_providerLabel(nullptr)
    , m_mainProvider(nullptr)
    , m_fallbackProvider(nullptr)
    , m_activeProvider(nullptr)
    , m_broadcastOnChange(true)
    , m_broadcastInterval(1000)
    , m_publishTopic("location.position")
    , m_requestTopic("location.request")
    , m_broadcastTimer(nullptr)
    , m_lastBroadcastTime(0)
    , m_useFallback(false)
{
    m_info.id = "location_communication";
    m_info.name = tr("Location");
    m_info.version = "0.1";
    m_info.description = tr("Location provider plugin - supports Serial GPS, GPSD, and manual entry with automatic failover");
    m_info.author = "LOUHI Team";
    m_info.type = PluginType::Communication;
    m_info.enabled = true;
    m_info.dependencies = QStringList();
    m_info.capabilities = QStringList() << "Serial GPS" << "GPSD" << "Manual" << "System Location" << "Failover" << "Request-Reply";
    m_info.subscribeTopics = QStringList() << "location.request";
    m_info.publishTopics = QStringList() << "location.position" << "location.position.reply";

    m_mainConfig.id = "manual_main";
    m_mainConfig.type = "manual";
    m_mainConfig.name = "Manual";
    m_mainConfig.enabled = true;
    m_mainConfig.providerConfig["latitude"] = 60.1699;
    m_mainConfig.providerConfig["longitude"] = 24.9384;
    m_mainConfig.providerConfig["altitude"] = 10.0;
    m_mainConfig.providerConfig["valid"] = true;

    m_fallbackConfig.id = "none";
    m_fallbackConfig.type = "none";
    m_fallbackConfig.name = "None";
    m_fallbackConfig.enabled = false;

    m_broadcastTimer = new QTimer(this);
    m_broadcastTimer->setSingleShot(false);
    connect(m_broadcastTimer, &QTimer::timeout, this, &LocationPlugin::onBroadcastTimer);
}

LocationPlugin::~LocationPlugin()
{
    unload();
}

PluginInfo LocationPlugin::getPluginInfo() const
{
    return m_info;
}

QVector<MenuEntry> LocationPlugin::getMenuEntries() const
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

bool LocationPlugin::load()
{
    qDebug() << "Location Plugin: Loading";
    return true;
}

bool LocationPlugin::initialize()
{
    qDebug() << "Location Plugin: Initializing";

    buildStatusWidget();

    if (m_mainProvider) {
        delete m_mainProvider;
        m_mainProvider = nullptr;
    }
    if (m_fallbackProvider) {
        delete m_fallbackProvider;
        m_fallbackProvider = nullptr;
    }

    m_mainProvider = createProvider(m_mainConfig);
    if (m_mainProvider) {
        connect(m_mainProvider, &GpsProvider::locationUpdated, this, &LocationPlugin::onLocationUpdated);
        connect(m_mainProvider, &GpsProvider::error, this, &LocationPlugin::onMainProviderError);
        connect(m_mainProvider, &GpsProvider::connected, this, [this]() {
            m_useFallback = false;
            m_activeProvider = m_mainProvider;
            updateStatusDisplay();
        });
        connect(m_mainProvider, &GpsProvider::disconnected, this, [this]() {
            if (m_useFallback) return;
            switchToFallback();
        });
    }

    if (m_fallbackConfig.enabled && m_fallbackConfig.type != "none") {
        m_fallbackProvider = createProvider(m_fallbackConfig);
        if (m_fallbackProvider) {
            connect(m_fallbackProvider, &GpsProvider::locationUpdated, this, &LocationPlugin::onLocationUpdated);
            connect(m_fallbackProvider, &GpsProvider::error, this, &LocationPlugin::onFallbackProviderError);
            connect(m_fallbackProvider, &GpsProvider::connected, this, [this]() {
                updateStatusDisplay();
            });
        }
    }

    m_activeProvider = m_mainProvider;
    m_useFallback = false;

    updateStatusDisplay();

    connect(this, &PluginInterface::statusChanged, this, [this](const QString& status) {
        if (status == "connect") {
            if (m_activeProvider && !m_activeProvider->isConnected()) {
                m_activeProvider->connect();
            }
        } else if (status == "disconnect") {
            if (m_mainProvider && m_mainProvider->isConnected()) {
                m_mainProvider->disconnect();
            }
            if (m_fallbackProvider && m_fallbackProvider->isConnected()) {
                m_fallbackProvider->disconnect();
            }
        }
    });

    return true;
}

bool LocationPlugin::start()
{
    qDebug() << "Location Plugin: Starting";

    if (m_activeProvider && !m_activeProvider->isConnected()) {
        m_activeProvider->connect();
    }

    m_broadcastTimer->start(m_broadcastInterval);

    return true;
}

bool LocationPlugin::stop()
{
    qDebug() << "Location Plugin: Stopping";

    m_broadcastTimer->stop();

    if (m_mainProvider && m_mainProvider->isConnected()) {
        m_mainProvider->disconnect();
    }
    if (m_fallbackProvider && m_fallbackProvider->isConnected()) {
        m_fallbackProvider->disconnect();
    }

    return true;
}

bool LocationPlugin::unload()
{
    qDebug() << "Location Plugin: Unloading";

    m_broadcastTimer->stop();

    if (m_mainProvider) {
        m_mainProvider->disconnect();
        m_mainProvider->deleteLater();
        m_mainProvider = nullptr;
    }
    if (m_fallbackProvider) {
        m_fallbackProvider->disconnect();
        m_fallbackProvider->deleteLater();
        m_fallbackProvider = nullptr;
    }
    m_activeProvider = nullptr;

    delete m_statusWidget;
    m_statusWidget = nullptr;

    return true;
}

QWidget* LocationPlugin::getWidget()
{
    return m_statusWidget;
}

void LocationPlugin::configure(QWidget* parent)
{
    LocationSettingsDialog dialog(parent);

    dialog.setMainProvider(m_mainConfig);
    dialog.setFallbackProvider(m_fallbackConfig);
    dialog.setBroadcastOnChange(m_broadcastOnChange);
    dialog.setBroadcastInterval(m_broadcastInterval);
    dialog.setPublishTopic(m_publishTopic);
    dialog.setRequestTopic(m_requestTopic);

    if (dialog.exec() == QDialog::Accepted) {
        m_mainConfig = dialog.mainProvider();
        m_fallbackConfig = dialog.fallbackProvider();
        m_broadcastOnChange = dialog.broadcastOnChange();
        m_broadcastInterval = dialog.broadcastInterval();
        m_publishTopic = dialog.publishTopic();
        m_requestTopic = dialog.requestTopic();

        m_broadcastTimer->setInterval(m_broadcastInterval);

        if (m_mainProvider) {
            m_mainProvider->disconnect();
            m_mainProvider->deleteLater();
        }
        if (m_fallbackProvider) {
            m_fallbackProvider->disconnect();
            m_fallbackProvider->deleteLater();
        }

        m_mainProvider = createProvider(m_mainConfig);
        if (m_mainProvider) {
            connect(m_mainProvider, &GpsProvider::locationUpdated, this, &LocationPlugin::onLocationUpdated);
            connect(m_mainProvider, &GpsProvider::error, this, &LocationPlugin::onMainProviderError);
            connect(m_mainProvider, &GpsProvider::connected, this, [this]() {
                m_useFallback = false;
                m_activeProvider = m_mainProvider;
                updateStatusDisplay();
            });
            connect(m_mainProvider, &GpsProvider::disconnected, this, [this]() {
                if (m_useFallback) return;
                switchToFallback();
            });
        }

        if (m_fallbackConfig.enabled && m_fallbackConfig.type != "none") {
            m_fallbackProvider = createProvider(m_fallbackConfig);
            if (m_fallbackProvider) {
                connect(m_fallbackProvider, &GpsProvider::locationUpdated, this, &LocationPlugin::onLocationUpdated);
                connect(m_fallbackProvider, &GpsProvider::error, this, &LocationPlugin::onFallbackProviderError);
            }
        }

        m_activeProvider = m_mainProvider;
        m_useFallback = false;

        updateStatusDisplay();

        if (m_activeProvider && !m_activeProvider->isConnected()) {
            m_activeProvider->connect();
        }
    }
}

QJsonObject LocationPlugin::getConfig() const
{
    QJsonObject config;
    config["mainProvider"] = QJsonObject{
        {"type", m_mainConfig.type},
        {"name", m_mainConfig.name},
        {"enabled", m_mainConfig.enabled},
        {"providerConfig", m_mainConfig.providerConfig}
    };
    config["fallbackProvider"] = QJsonObject{
        {"type", m_fallbackConfig.type},
        {"name", m_fallbackConfig.name},
        {"enabled", m_fallbackConfig.enabled},
        {"providerConfig", m_fallbackConfig.providerConfig}
    };
    config["broadcastOnChange"] = m_broadcastOnChange;
    config["broadcastInterval"] = m_broadcastInterval;
    config["publishTopic"] = m_publishTopic;
    config["requestTopic"] = m_requestTopic;
    return config;
}

void LocationPlugin::setConfig(const QJsonObject& config)
{
    if (config.contains("mainProvider")) {
        QJsonObject mainObj = config.value("mainProvider").toObject();
        m_mainConfig.type = mainObj.value("type").toString("manual");
        m_mainConfig.name = mainObj.value("name").toString("Manual");
        m_mainConfig.enabled = mainObj.value("enabled").toBool(true);
        m_mainConfig.providerConfig = mainObj.value("providerConfig").toObject();
        m_mainConfig.id = m_mainConfig.type + "_" + m_mainConfig.name.toLower().replace(" ", "_");
    }

    if (config.contains("fallbackProvider")) {
        QJsonObject fallbackObj = config.value("fallbackProvider").toObject();
        m_fallbackConfig.type = fallbackObj.value("type").toString("none");
        m_fallbackConfig.name = fallbackObj.value("name").toString("None");
        m_fallbackConfig.enabled = fallbackObj.value("enabled").toBool(false);
        m_fallbackConfig.providerConfig = fallbackObj.value("providerConfig").toObject();
        m_fallbackConfig.id = m_fallbackConfig.type + "_" + m_fallbackConfig.name.toLower().replace(" ", "_");
    }

    m_broadcastOnChange = config.value("broadcastOnChange").toBool(true);
    m_broadcastInterval = config.value("broadcastInterval").toInt(1000);
    m_publishTopic = config.value("publishTopic").toString("location.position");
    m_requestTopic = config.value("requestTopic").toString("location.request");
}

void LocationPlugin::setSubscribedTopics(const QStringList& topics)
{
    m_subscribedTopics = topics;
    qDebug() << "Location Plugin: Subscribed topics:" << topics;
}

void LocationPlugin::onLocationUpdated(const LocationData& location)
{
    updateStatusDisplay();

    if (m_broadcastOnChange) {
        qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (now - m_lastBroadcastTime >= m_broadcastInterval) {
            broadcastLocation(location);
        }
    }
}

void LocationPlugin::onMainProviderError(const QString& error)
{
    qDebug() << "Location Plugin: Main provider error:" << error;
    emit statusChanged("Main provider error: " + error);

    if (m_fallbackProvider && m_fallbackConfig.enabled) {
        switchToFallback();
    }
}

void LocationPlugin::onFallbackProviderError(const QString& error)
{
    qDebug() << "Location Plugin: Fallback provider error:" << error;
    emit statusChanged("Fallback provider error: " + error);
}

void LocationPlugin::onLocationRequest(const QString& topic, const QString& payload)
{
    Q_UNUSED(topic);
    Q_UNUSED(payload);

    if (m_activeProvider && m_activeProvider->isConnected()) {
        LocationData location = m_activeProvider->getCurrentLocation();
        broadcastLocation(location, true);
    }
}

void LocationPlugin::onBroadcastTimer()
{
    if (m_activeProvider && m_activeProvider->isConnected()) {
        LocationData location = m_activeProvider->getCurrentLocation();
        if (location.valid) {
            broadcastLocation(location);
        }
    }
}

void LocationPlugin::buildStatusWidget()
{
    m_statusWidget = new QWidget();
    m_mainLayout = new QVBoxLayout(m_statusWidget);

    QGroupBox* statusGroup = new QGroupBox(tr("Location Status"), m_statusWidget);
    QVBoxLayout* groupLayout = new QVBoxLayout(statusGroup);

    m_providerLabel = new QLabel(tr("Provider: Not configured"), statusGroup);
    m_statusLabel = new QLabel(tr("Status: Not connected"), statusGroup);
    m_locationLabel = new QLabel(tr("Location: N/A"), statusGroup);

    groupLayout->addWidget(m_providerLabel);
    groupLayout->addWidget(m_statusLabel);
    groupLayout->addWidget(m_locationLabel);

    QPushButton* configureBtn = new QPushButton(tr("Configure..."), statusGroup);
    groupLayout->addWidget(configureBtn);

    m_mainLayout->addWidget(statusGroup);
    m_mainLayout->addStretch();

    connect(configureBtn, &QPushButton::clicked, this, [this]() {
        configure(m_statusWidget);
    });
}

void LocationPlugin::updateStatusDisplay()
{
    if (!m_statusWidget) return;

    QString providerName = tr("None");
    QString statusText = tr("Not connected");
    QString locationText = tr("N/A");

    if (m_activeProvider) {
        providerName = m_activeProvider->providerId();
        if (m_useFallback) {
            providerName += tr(" (fallback)");
        }

        if (m_activeProvider->isConnected()) {
            statusText = tr("Connected");
            LocationData loc = m_activeProvider->getCurrentLocation();
            if (loc.valid) {
                locationText = QString("Lat: %1, Lon: %2, Alt: %3m")
                    .arg(loc.latitude, 0, 'f', 6)
                    .arg(loc.longitude, 0, 'f', 6)
                    .arg(loc.altitude, 0, 'f', 1);
            } else {
                locationText = tr("No fix");
            }
        } else {
            statusText = tr("Disconnected");
        }
    }

    m_providerLabel->setText(tr("Provider: %1").arg(providerName));
    m_statusLabel->setText(tr("Status: %1").arg(statusText));
    m_locationLabel->setText(tr("Location: %1").arg(locationText));

    QString connStatus = m_activeProvider && m_activeProvider->isConnected() ? tr("Connected") : tr("Disconnected");
    emit connectionStatusChanged("Location", connStatus);
}

void LocationPlugin::switchToFallback()
{
    if (!m_fallbackProvider || !m_fallbackConfig.enabled) {
        qDebug() << "Location Plugin: No fallback provider available";
        return;
    }

    qDebug() << "Location Plugin: Switching to fallback provider";
    m_useFallback = true;
    m_activeProvider = m_fallbackProvider;

    if (!m_activeProvider->isConnected()) {
        m_activeProvider->connect();
    }

    updateStatusDisplay();
}

void LocationPlugin::switchToMain()
{
    if (!m_mainProvider) return;

    qDebug() << "Location Plugin: Switching back to main provider";
    m_useFallback = false;
    m_activeProvider = m_mainProvider;

    if (!m_activeProvider->isConnected()) {
        m_activeProvider->connect();
    }

    if (m_fallbackProvider && m_fallbackProvider->isConnected()) {
        m_fallbackProvider->disconnect();
    }

    updateStatusDisplay();
}

void LocationPlugin::broadcastLocation(const LocationData& location, bool isReply)
{
    if (!location.valid) return;

    QJsonObject locJson;
    locJson["latitude"] = location.latitude;
    locJson["longitude"] = location.longitude;
    locJson["altitude"] = location.altitude;
    locJson["speed"] = location.speed;
    locJson["course"] = location.course;
    locJson["hdop"] = location.hdop;
    locJson["satellites"] = location.satellites;
    locJson["source"] = location.source;
    locJson["timestamp"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

    QString topic = isReply ? m_publishTopic + ".reply" : m_publishTopic;
    QString payload = QString::fromUtf8(QJsonDocument(locJson).toJson(QJsonDocument::Compact));

    emit messageReceived(topic, payload);

    m_lastBroadcastLocation = location;
    m_lastBroadcastTime = QDateTime::currentMSecsSinceEpoch();
}

GpsProvider* LocationPlugin::createProvider(const LocationProviderConfig& config)
{
    if (config.type == "serial") {
        SerialGpsProvider* provider = new SerialGpsProvider(this);
        provider->setConfig(config.providerConfig);
        return provider;
    } else if (config.type == "gpsd") {
        GpsdProvider* provider = new GpsdProvider(this);
        provider->setConfig(config.providerConfig);
        return provider;
    } else if (config.type == "manual") {
        ManualProvider* provider = new ManualProvider(this);
        provider->setConfig(config.providerConfig);
        return provider;
    } else if (config.type == "system") {
#ifdef QT_POSITIONING_LIB
        SystemPositionProvider* provider = new SystemPositionProvider(this);
        provider->setConfig(config.providerConfig);
        return provider;
#else
        qWarning() << "Location Plugin: System location provider not available (Qt5::Positioning missing)";
        return nullptr;
#endif
    }

    return nullptr;
}
