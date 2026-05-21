#ifndef PLUGINMANAGER_H
#define PLUGINMANAGER_H

#include <QObject>
#include <QVector>
#include <QMap>
#include <QString>
#include <QPluginLoader>
#include <QJsonObject>
#include "plugininterface.h"
#include "configmanager.h"

class QMenuBar;
class QMainWindow;

struct LoadedPlugin {
    QString filePath;
    PluginInterface* plugin;
    QPluginLoader* loader;
    bool enabled;
};

class PluginManager : public QObject
{
    Q_OBJECT

public:
    explicit PluginManager(QObject* parent = nullptr);
    ~PluginManager();

    void setConfigManager(ConfigManager* configManager);

    void discoverPlugins(const QString& pluginDir);
    void loadAllPlugins();
    void unloadAllPlugins();

    QStringList collectAllSubscribeTopics() const;
    void updateCommunicationPluginTopics();

    QVector<LoadedPlugin> getLoadedPlugins() const { return m_plugins; }
    QVector<PluginInterface*> getPluginsByType(PluginType type) const;
    QVector<PluginInterface*> getAllPlugins() const;
    QVector<PluginInterface*> getEnabledPlugins() const;

    void enablePlugin(const QString& pluginId);
    void disablePlugin(const QString& pluginId);

    void setupMenu(QMenuBar* menuBar);
    void broadcastMessage(const QString& topic, const QString& payload, PluginInterface* sender = nullptr);

    void emitMessageToPlugins(const QString& topic, const QString& payload);

signals:
    void pluginConnectionStatusChanged(const QString& pluginId, const QString& status);
    void pluginMessageReceived(const QString& pluginId, const QString& topic, const QString& payload);

private:
    QVector<LoadedPlugin> m_plugins;
    QMap<QString, QVector<PluginInterface*>> m_topicSubscribers;
    ConfigManager* m_configManager;
    QString m_pluginDir;
};

#endif