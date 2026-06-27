#include "pluginmanager.h"
#include <QMenuBar>
#include <QDir>
#include <QDebug>
#include <QMetaMethod>

static bool matchesNatsTopic(const QString& subscription, const QString& topic)
{
    if (subscription == ">") {
        return true;
    }

    QStringList subParts = subscription.split('.');
    QStringList topicParts = topic.split('.');

    for (int i = 0; i < subParts.size(); ++i) {
        if (subParts[i] == ">") {
            return true;
        }

        if (i >= topicParts.size()) {
            return false;
        }

        if (subParts[i] != "*" && subParts[i] != topicParts[i]) {
            return false;
        }
    }

    return subParts.size() == topicParts.size();
}

PluginManager::PluginManager(QObject* parent)
    : QObject(parent)
    , m_configManager(nullptr)
{
}

void PluginManager::setEmcon(bool active)
{
    if (m_emconActive == active) return;
    m_emconActive = active;
    emit emconChanged(active);
    qDebug() << "PluginManager: EMCON" << (active ? "activated" : "deactivated");
}

PluginManager::~PluginManager()
{
    unloadAllPlugins();
}

void PluginManager::setConfigManager(ConfigManager* configManager)
{
    m_configManager = configManager;
}

void PluginManager::discoverPlugins(const QString& pluginDir)
{
    m_pluginDir = pluginDir;
    QDir dir(pluginDir);

    if (!dir.exists()) {
        qWarning() << "Plugin directory does not exist:" << pluginDir;
        return;
    }

    QStringList filters;
#ifdef Q_OS_WIN
    filters << "*.dll";
#elif defined(Q_OS_MACOS)
    filters << "*.dylib" << "*.so";
#else
    filters << "*.so";
#endif
    QFileInfoList files = dir.entryInfoList(filters, QDir::Files);

    for (const QFileInfo& fileInfo : files) {
        QString filePath = fileInfo.absoluteFilePath();

        QPluginLoader* loader = new QPluginLoader(filePath, this);

        if (!loader->metaData().contains("MetaData")) {
            qDebug() << "No metadata in plugin:" << filePath;
            delete loader;
            continue;
        }

        QJsonObject meta = loader->metaData().value("MetaData").toObject();
        QString iid = meta.value("IID").toString();

        if (iid != PluginInterface_iid) {
            qDebug() << "Plugin has wrong interface:" << filePath << iid;
            delete loader;
            continue;
        }

        LoadedPlugin loaded;
        loaded.filePath = filePath;
        loaded.loader = loader;
        loaded.plugin = nullptr;
        loaded.enabled = false;

        m_plugins.append(loaded);
        qDebug() << "Discovered plugin:" << filePath;
    }
}

void PluginManager::loadAllPlugins()
{
    for (LoadedPlugin& loaded : m_plugins) {
        QObject* instance = loaded.loader->instance();
        if (!instance) {
            qWarning() << "Failed to load plugin:" << loaded.filePath << loaded.loader->errorString();
            continue;
        }

        PluginInterface* plugin = qobject_cast<PluginInterface*>(instance);
        if (!plugin) {
            qWarning() << "Plugin does not implement PluginInterface:" << loaded.filePath;
            continue;
        }

        loaded.plugin = plugin;

        PluginInfo info = plugin->getPluginInfo();
        info.id = info.name.toLower().replace(" ", "_");

        QJsonObject pluginConfig = m_configManager ?
            m_configManager->getPluginConfig(info.id) : QJsonObject();

        qDebug() << "PluginManager: Loading config for" << info.name << "(id:" << info.id << ") keys:" << pluginConfig.keys();

        if (!pluginConfig.isEmpty()) {
            plugin->setConfig(pluginConfig);
        }

        if (plugin->load()) {
            loaded.enabled = true;
            connect(plugin, &PluginInterface::messageReceived,
                    this, &PluginManager::emitMessageToPlugins);
            connect(plugin, &PluginInterface::connectionStatusChanged,
                    this, [this, plugin](const QString& connectionName, const QString& status) {
                        emit pluginConnectionStatusChanged(plugin->getPluginInfo().id, status);
                    });
            connect(plugin, &PluginInterface::configChanged,
                    this, [this, plugin]() {
                        emit pluginConfigChanged(plugin->getPluginInfo().id);
                    });
            qDebug() << "Loaded plugin:" << info.name;
        }
    }

    updateCommunicationPluginTopics();
}

void PluginManager::unloadAllPlugins()
{
    for (LoadedPlugin& loaded : m_plugins) {
        if (loaded.plugin) {
            loaded.plugin->stop();
            loaded.plugin->unload();
        }
        if (loaded.loader) {
            loaded.loader->unload();
        }
    }
    m_plugins.clear();
}

QVector<PluginInterface*> PluginManager::getPluginsByType(PluginType type) const
{
    QVector<PluginInterface*> result;
    for (const LoadedPlugin& loaded : m_plugins) {
        if (loaded.plugin && loaded.enabled &&
            loaded.plugin->getPluginInfo().type == type) {
            result.append(loaded.plugin);
        }
    }
    return result;
}

QVector<PluginInterface*> PluginManager::getAllPlugins() const
{
    QVector<PluginInterface*> result;
    for (const LoadedPlugin& loaded : m_plugins) {
        if (loaded.plugin) {
            result.append(loaded.plugin);
        }
    }
    return result;
}

QVector<PluginInterface*> PluginManager::getEnabledPlugins() const
{
    QVector<PluginInterface*> result;
    for (const LoadedPlugin& loaded : m_plugins) {
        if (loaded.plugin && loaded.enabled) {
            result.append(loaded.plugin);
        }
    }
    return result;
}

void PluginManager::enablePlugin(const QString& pluginId)
{
    for (LoadedPlugin& loaded : m_plugins) {
        if (loaded.plugin && loaded.plugin->getPluginInfo().id == pluginId) {
            if (!loaded.enabled) {
                if (loaded.plugin->initialize() && loaded.plugin->start()) {
                    loaded.enabled = true;
                    qDebug() << "Enabled plugin:" << pluginId;
                }
            }
            break;
        }
    }
}

void PluginManager::disablePlugin(const QString& pluginId)
{
    for (LoadedPlugin& loaded : m_plugins) {
        if (loaded.plugin && loaded.plugin->getPluginInfo().id == pluginId) {
            if (loaded.enabled) {
                loaded.plugin->stop();
                loaded.enabled = false;
                qDebug() << "Disabled plugin:" << pluginId;
            }
            break;
        }
    }
}

QVector<ToolbarEntry> PluginManager::collectToolbarEntries() const
{
    QVector<ToolbarEntry> entries;
    for (const LoadedPlugin& loaded : m_plugins) {
        if (!loaded.plugin || !loaded.enabled) continue;
        entries.append(loaded.plugin->getToolbarEntries());
    }
    return entries;
}

void PluginManager::setupMenu(QMenuBar* menuBar)
{
    QMap<QString, QMenu*> menus;

    for (QAction* action : menuBar->actions()) {
        if (action->menu()) {
            menus[action->text()] = action->menu();
            action->menu()->clear();
        }
    }

    for (const LoadedPlugin& loaded : m_plugins) {
        if (!loaded.plugin || !loaded.enabled) continue;

        PluginInterface* plugin = loaded.plugin;

        for (const MenuEntry& entry : plugin->getMenuEntries()) {
            QString topMenu = entry.topMenu.isEmpty() ?
                plugin->getPluginInfo().name : entry.topMenu;

            if (!menus.contains(topMenu)) {
                menus[topMenu] = menuBar->addMenu(topMenu);
            }

            if (entry.addAsDirectAction) {
                for (const QString& sub : entry.subMenus) {
                    QAction* action = menus[topMenu]->addAction(sub);
                    connect(action, &QAction::triggered, this, [this, plugin, sub, topMenu]() {
                        if (sub == "Settings" || topMenu == tr("Settings")) {
                            plugin->configure(nullptr);
                        } else if (sub.startsWith("Show")) {
                            emit plugin->showWidgetRequested();
                        } else if (sub == "Connect") {
                            emit plugin->statusChanged("connect");
                        } else if (sub == "Disconnect") {
                            emit plugin->statusChanged("disconnect");
                        } else if (sub == tr("Basemap")) {
                            plugin->handleToolbarAction("osgearth_basemap");
                        }
                    });
                }
            } else if (entry.subMenus.isEmpty()) {
                menus[topMenu]->addAction(plugin->getPluginInfo().name,
                    this, [this, plugin]() {
                        plugin->configure(nullptr);
                    });
            } else {
                QMenu* subMenu = menus[topMenu]->addMenu(plugin->getPluginInfo().name);
                for (const QString& sub : entry.subMenus) {
                    QAction* action = subMenu->addAction(sub);
                    connect(action, &QAction::triggered, this, [this, plugin, sub, topMenu]() {
                        if (sub == "Settings" || topMenu == tr("Settings")) {
                            plugin->configure(nullptr);
                        } else if (sub.startsWith("Show")) {
                            emit plugin->showWidgetRequested();
                        } else if (sub == "Connect") {
                            emit plugin->statusChanged("connect");
                        } else if (sub == "Disconnect") {
                            emit plugin->statusChanged("disconnect");
                        }
                    });
                }
            }
        }
    }

    QString settingsLabel = tr("Settings");
    bool hasSettings = false;
    for (QAction* action : menuBar->actions()) {
        if (action->text() == settingsLabel) {
            hasSettings = true;
            break;
        }
    }

    if (!hasSettings) {
        QMenu* settingsMenu = menuBar->addMenu(settingsLabel);
        for (PluginInterface* plugin : getEnabledPlugins()) {
            QAction* sa = settingsMenu->addAction(plugin->getPluginInfo().name);
            connect(sa, &QAction::triggered, this, [this, plugin]() {
                plugin->configure(nullptr);
            });
        }
    }
}

void PluginManager::broadcastMessage(const QString& topic, const QString& payload, PluginInterface* sender)
{
    for (const LoadedPlugin& loaded : m_plugins) {
        if (!loaded.plugin || !loaded.enabled) continue;
        if (loaded.plugin == sender) continue;

        const PluginInfo& info = loaded.plugin->getPluginInfo();
        for (const QString& sub : info.subscribeTopics) {
            if (matchesNatsTopic(sub, topic)) {
                loaded.plugin->deliverMessage(topic, payload);
                break;
            }
        }
    }
}

void PluginManager::emitMessageToPlugins(const QString& topic, const QString& payload)
{
    PluginInterface* sourcePlugin = qobject_cast<PluginInterface*>(sender());
    if (sourcePlugin) {
        emit pluginMessageReceived(sourcePlugin->getPluginInfo().id, topic, payload);

        // Outbound: route to communication plugins if sender allows it
        PluginInfo info = sourcePlugin->getPluginInfo();
        bool shouldPublish = info.publishToBackend;

        // Allow per-plugin config override
        if (shouldPublish && m_configManager) {
            QJsonObject pluginCfg = m_configManager->getPluginConfig(info.id);
            if (pluginCfg.contains("publishToBackend")) {
                shouldPublish = pluginCfg.value("publishToBackend").toBool();
            }
        }

        if (shouldPublish && !m_emconActive) {
            for (const LoadedPlugin& loaded : m_plugins) {
                if (!loaded.plugin || !loaded.enabled) continue;
                if (loaded.plugin == sourcePlugin) continue;
                if (loaded.plugin->getPluginInfo().type == PluginType::Communication) {
                    loaded.plugin->publish(topic, payload);
                }
            }
        }
    }
    broadcastMessage(topic, payload, sourcePlugin);
}

QStringList PluginManager::collectAllSubscribeTopics() const
{
    QStringList allTopics;
    for (const LoadedPlugin& loaded : m_plugins) {
        if (!loaded.plugin || !loaded.enabled) continue;
        if (loaded.plugin->getPluginInfo().type == PluginType::Communication) continue;

        for (const QString& topic : loaded.plugin->getPluginInfo().subscribeTopics) {
            if (!allTopics.contains(topic)) {
                allTopics.append(topic);
            }
        }
    }
    return allTopics;
}

void PluginManager::updateCommunicationPluginTopics()
{
    QStringList topics = collectAllSubscribeTopics();
    qDebug() << "PluginManager: Aggregated subscribe topics from all plugins:" << topics;

    for (const LoadedPlugin& loaded : m_plugins) {
        if (!loaded.plugin || !loaded.enabled) continue;
        if (loaded.plugin->getPluginInfo().type == PluginType::Communication) {
            loaded.plugin->setSubscribedTopics(topics);
        }
    }
}