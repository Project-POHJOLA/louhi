#ifndef TAKPLUGIN_H
#define TAKPLUGIN_H

#include "../src/plugininterface.h"
#include "takserverconnection.h"
#include <QMap>
#include <QJsonObject>
#include <QUuid>
#include <QtPlugin>

class QWidget;
class QVBoxLayout;
class QListWidget;
class QLabel;
class TakSettingsDialog;

class TakPlugin : public PluginInterface
{
    Q_OBJECT
    Q_INTERFACES(PluginInterface)
    Q_PLUGIN_METADATA(IID "com.louhi.plugininterface/1.0" FILE "takplugin.json")

public:
    TakPlugin(QObject* parent = nullptr);
    ~TakPlugin();

    PluginInfo getPluginInfo() const override;
    QVector<MenuEntry> getMenuEntries() const override;

    bool load() override;
    bool initialize() override;
    bool start() override;
    bool stop() override;
    bool unload() override;

    QWidget* getWidget() override;
    void configure(QWidget* parent) override;

    QJsonObject getConfig() const override;
    void setConfig(const QJsonObject& config) override;

    void deliverMessage(const QString& topic, const QString& payload) override;

private slots:
    void onServerConnected();
    void onServerDisconnected();
    void onServerMessageReceived(const QString& xml);
    void onServerError(const QString& error);
    void onServerStatusChanged(const QString& status);

private:
    void buildStatusWidget();
    void updateStatusDisplay();
    void connectToServer(const QString& serverId);
    void disconnectFromServer(const QString& serverId);
    void publishToNats(const QString& serverId, const QString& xml);
    QString sanitizeTopicName(const QString& input) const;

    TakServerConfig configFromJson(const QJsonObject& obj) const;
    QJsonObject configToJson(const TakServerConfig& config) const;

    void sendCoTToAllServers(const QString& xml);

    PluginInfo m_info;
    QWidget* m_statusWidget;
    QVBoxLayout* m_mainLayout;
    QListWidget* m_serverStatusList;
    QLabel* m_connectionLabel;

    QList<TakServerConfig> m_serverConfigs;
    QMap<QString, TakServerConnection*> m_connections;

    QString m_deviceUid;
};

#endif
