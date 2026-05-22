#include "mainwindow.h"
#include "mainwindow.moc"
#include "plugininterface.h"
#include "version.h"
#include <QMenuBar>
#include <QDebug>
#include <QLabel>
#include <QJsonArray>
#include <QMessageBox>
#include <QIcon>
#include <QApplication>
#include <QMenu>
#include <QProcess>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_configManager(new ConfigManager(this))
    , m_pluginManager(new PluginManager(this))
    , m_ledManager(new ConnectionLedManager(statusBar(), this))
{
    m_pluginManager->setConfigManager(m_configManager);

    setWindowTitle(tr("LOUHI v%1 - Battle Management System").arg(LOUHI_VERSION_STRING));
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

    m_mainToolBar = new QToolBar(tr("Main"), this);
    addToolBar(Qt::RightToolBarArea, m_mainToolBar);
    m_mainToolBar->setObjectName("mainToolBar");
    m_mainToolBar->setMovable(false);

    statusBar()->showMessage(tr("Ready"));

    connect(m_pluginManager, &PluginManager::pluginConnectionStatusChanged,
            m_ledManager, &ConnectionLedManager::onPluginStatusChanged);
    connect(m_pluginManager, &PluginManager::pluginMessageReceived,
            m_ledManager, &ConnectionLedManager::onPluginMessageReceived);
    connect(m_pluginManager, &PluginManager::pluginConfigChanged,
            this, &MainWindow::onPluginConfigChanged);
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

static QWidget* createSpacer(int width)
{
    QWidget* w = new QWidget;
    w->setFixedWidth(width);
    return w;
}

void MainWindow::setupToolbar()
{
    m_mainToolBar->setVisible(true);
    m_mainToolBar->clear();

    QAction* aboutAction = m_mainToolBar->addAction(tr("About"));
    aboutAction->setToolTip(tr("About LOUHI"));
    connect(aboutAction, &QAction::triggered, this, &MainWindow::showAbout);

    m_mainToolBar->addWidget(createSpacer(8));

    QAction* exitAction = m_mainToolBar->addAction(tr("Exit"));
    exitAction->setToolTip(tr("Exit LOUHI"));
    connect(exitAction, &QAction::triggered, qApp, &QApplication::quit);

    QStringList groups;
    QMap<QString, QVector<ToolbarEntry>> grouped;
    for (const ToolbarEntry& entry : m_pluginManager->collectToolbarEntries()) {
        QString g = entry.group.isEmpty() ? QString() : entry.group;
        grouped[g].append(entry);
        if (!g.isEmpty() && !groups.contains(g))
            groups.append(g);
    }

    if (!grouped[QString()].isEmpty())
        groups.prepend(QString());

    for (const QString& group : groups) {
        m_mainToolBar->addWidget(createSpacer(6));
        for (const ToolbarEntry& entry : grouped[group]) {
            QAction* action = m_mainToolBar->addAction(entry.text);
            action->setObjectName(entry.id);
            if (!entry.tooltip.isEmpty())
                action->setToolTip(entry.tooltip);
            if (!entry.iconPath.isEmpty())
                action->setIcon(QIcon(entry.iconPath));

            PluginInterface* sourcePlugin = nullptr;
            for (PluginInterface* plugin : m_pluginManager->getEnabledPlugins()) {
                for (const ToolbarEntry& pe : plugin->getToolbarEntries()) {
                    if (pe.id == entry.id) {
                        sourcePlugin = plugin;
                        break;
                    }
                }
                if (sourcePlugin) break;
            }

            if (sourcePlugin) {
                QString actionId = entry.id;
                connect(action, &QAction::triggered, this, [sourcePlugin, actionId]() {
                    sourcePlugin->handleToolbarAction(actionId);
                });
            }
        }
    }
}

void MainWindow::showAbout()
{
    QMessageBox::about(this, tr("About LOUHI"),
        QString("<h3>") + tr("LOUHI v%1").arg(LOUHI_VERSION_STRING) +
            QString("</h3><p>") + tr("Battle Management System") +
            QString("</p><p>") + tr("A modern, plugin-based BMS supporting "
                "NATS messaging, TAK communication, "
                "location services, and map visualization.") +
            QString("</p>"));
}

void MainWindow::showPluginManager()
{
    PluginManagerDialog dialog(m_pluginManager, this);
    dialog.exec();

    m_pluginManager->setupMenu(menuBar());
    setupConnectionLeds();
    setupToolbar();
}

void MainWindow::setupLanguageMenu()
{
    QMenu* langMenu = menuBar()->addMenu(tr("Language"));

    QActionGroup* langGroup = new QActionGroup(this);
    langGroup->setExclusive(true);

    struct LangEntry { QString code; QString label; };
    QVector<LangEntry> langs = {
        {"en", "English"},
        {"de", "Deutsch"},
        {"fi", "Suomi"},
        {"sv", "Svenska"}
    };

    QString currentLang = getConfigManager()->getAppConfig().value("language").toString();

    for (const LangEntry& le : langs) {
        QAction* action = langMenu->addAction(le.label);
        action->setData(le.code);
        action->setCheckable(true);
        action->setChecked(le.code == currentLang || (currentLang.isEmpty() && le.code == "en"));
        langGroup->addAction(action);
    }

    connect(langGroup, &QActionGroup::triggered, this, [this](QAction* action) {
        QString langCode = action->data().toString();
        changeLanguage(langCode);
    });
}

void MainWindow::changeLanguage(const QString& langCode)
{
    QJsonObject appConfig = getConfigManager()->getAppConfig();
    appConfig["dockState"] = QString::fromUtf8(saveState().toBase64());
    appConfig["language"] = langCode;
    getConfigManager()->setAppConfig(appConfig);
    getConfigManager()->saveConfig();

    QProcess::startDetached(QApplication::applicationFilePath(), QApplication::arguments());
    QApplication::quit();
}

void MainWindow::onPluginConfigChanged(const QString& pluginId)
{
    for (PluginInterface* plugin : m_pluginManager->getEnabledPlugins()) {
        if (plugin->getPluginInfo().id == pluginId) {
            m_configManager->setPluginConfig(pluginId, plugin->getConfig());
            break;
        }
    }
    m_configManager->saveConfig();
    setupConnectionLeds();
}

void MainWindow::setupConnectionLeds()
{
    m_ledManager->clearAllLeds();

    for (PluginInterface* plugin : m_pluginManager->getPluginsByType(PluginType::Communication)) {
        PluginInfo info = plugin->getPluginInfo();
        QString ledId = info.id;

        if (info.id == "nats_communication") {
            QJsonObject config = plugin->getConfig();
            if (config.contains("servers")) {
                QJsonArray servers = config.value("servers").toArray();
                for (const QJsonValue& v : servers) {
                    QJsonObject server = v.toObject();
                    QString serverName = server.value("name").toString(tr("Unnamed"));
                    QString serverLedId = ledId + "_" + serverName;
                    m_ledManager->addLed(serverLedId, "NATS", serverName);
                }
            } else {
                m_ledManager->addLed(ledId, "NATS", tr("Default"));
            }
        } else if (info.id == "tak_communication") {
            QJsonObject config = plugin->getConfig();
            if (config.contains("servers")) {
                QJsonArray servers = config.value("servers").toArray();
                for (const QJsonValue& v : servers) {
                    QJsonObject server = v.toObject();
                    QString serverName = server.value("name").toString(tr("Unnamed"));
                    QString serverLedId = ledId + "_" + serverName;
                    m_ledManager->addLed(serverLedId, "TAK", serverName);
                }
            } else {
                m_ledManager->addLed(ledId, "TAK", tr("Default"));
            }
        } else if (info.id == "location_communication") {
            QJsonObject config = plugin->getConfig();
            QString mainProvider = tr("Manual");
            if (config.contains("mainProvider")) {
                QJsonObject mainObj = config.value("mainProvider").toObject();
                mainProvider = mainObj.value("name").toString(tr("Manual"));
            }
            m_ledManager->addLed(ledId, "Location", mainProvider);
        }
    }
}
