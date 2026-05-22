#include "mainwindow.h"
#include "mainwindow.moc"
#include "plugininterface.h"
#include "version.h"
#include <QMenuBar>
#include <QDebug>
#include <QLabel>
#include <QJsonArray>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_configManager(new ConfigManager(this))
    , m_pluginManager(new PluginManager(this))
    , m_ledManager(new ConnectionLedManager(statusBar(), this))
{
    m_pluginManager->setConfigManager(m_configManager);

    setWindowTitle(QString("LOUHI v%1 - Battle Management System").arg(LOUHI_VERSION_STRING));
    resize(1024, 768);

    setDockNestingEnabled(true);

    m_configManager->loadConfig();

    QJsonObject appConfig = m_configManager->getAppConfig();
    if (appConfig.contains("window")) {
        QJsonObject window = appConfig.value("window").toObject();
        if (window.contains("width") && window.contains("height")) {
            resize(window.value("width").toInt(), window.value("height").toInt());
        }
        if (window.contains("x") && window.contains("y")) {
            move(window.value("x").toInt(), window.value("y").toInt());
        }
    }

    if (appConfig.contains("dockState")) {
        QByteArray encoded = QByteArray::fromBase64(
            appConfig.value("dockState").toString().toUtf8());
        m_pendingDockState = encoded;
    } else {
        m_pendingDockState = QByteArray();
    }

    statusBar()->showMessage("Ready");

    connect(m_pluginManager, &PluginManager::pluginConnectionStatusChanged,
            m_ledManager, &ConnectionLedManager::onPluginStatusChanged);
    connect(m_pluginManager, &PluginManager::pluginMessageReceived,
            m_ledManager, &ConnectionLedManager::onPluginMessageReceived);
}

MainWindow::~MainWindow()
{
    QJsonObject appConfig = m_configManager->getAppConfig();
    QJsonObject window;
    window["width"] = width();
    window["height"] = height();
    window["x"] = x();
    window["y"] = y();
    appConfig["window"] = window;
    appConfig["dockState"] = QString::fromUtf8(saveState().toBase64());
    m_configManager->setAppConfig(appConfig);

    for (PluginInterface* plugin : m_pluginManager->getEnabledPlugins()) {
        PluginInfo info = plugin->getPluginInfo();
        qDebug() << "MainWindow: Saving config for plugin" << info.id;
        m_configManager->setPluginConfig(info.id, plugin->getConfig());
    }

    m_configManager->saveConfig();
    qDebug() << "MainWindow: Config saved to" << m_configManager->configFilePath();
    m_pluginManager->unloadAllPlugins();
}

bool MainWindow::restoreDockState()
{
    if (!m_pendingDockState.isEmpty()) {
        bool ok = restoreState(m_pendingDockState);
        m_pendingDockState.clear();
        return ok;
    }
    return false;
}

void MainWindow::showPluginManager()
{
    PluginManagerDialog dialog(m_pluginManager, this);
    dialog.exec();

    m_pluginManager->setupMenu(menuBar());
    setupConnectionLeds();
}

void MainWindow::setupConnectionLeds()
{
    for (PluginInterface* plugin : m_pluginManager->getPluginsByType(PluginType::Communication)) {
        PluginInfo info = plugin->getPluginInfo();
        QString ledId = info.id;

        if (info.id == "nats_communication") {
            QJsonObject config = plugin->getConfig();
            if (config.contains("servers")) {
                QJsonArray servers = config.value("servers").toArray();
                for (const QJsonValue& v : servers) {
                    QJsonObject server = v.toObject();
                    QString serverName = server.value("name").toString("Unnamed");
                    QString serverLedId = ledId + "_" + serverName;
                    m_ledManager->addLed(serverLedId, "NATS", serverName);
                }
            } else {
                m_ledManager->addLed(ledId, "NATS", "Default");
            }
        } else if (info.id == "tak_communication") {
            QJsonObject config = plugin->getConfig();
            if (config.contains("servers")) {
                QJsonArray servers = config.value("servers").toArray();
                for (const QJsonValue& v : servers) {
                    QJsonObject server = v.toObject();
                    QString serverName = server.value("name").toString("Unnamed");
                    QString serverLedId = ledId + "_" + serverName;
                    m_ledManager->addLed(serverLedId, "TAK", serverName);
                }
            } else {
                m_ledManager->addLed(ledId, "TAK", "Default");
            }
        } else if (info.id == "location_communication") {
            QJsonObject config = plugin->getConfig();
            QString mainProvider = "Manual";
            if (config.contains("mainProvider")) {
                QJsonObject mainObj = config.value("mainProvider").toObject();
                mainProvider = mainObj.value("name").toString("Manual");
            }
            m_ledManager->addLed(ledId, "Location", mainProvider);
        }
    }
}
