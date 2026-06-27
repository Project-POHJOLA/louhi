#ifndef NATSPLUGIN_H
#define NATSPLUGIN_H

#include "../src/plugininterface.h"
#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <QMap>
#include <QList>
#include <QtPlugin>

class QWidget;
class QVBoxLayout;
class QListWidget;
class QLabel;
class NatsClient;
class NatsSettingsDialog;

struct NatsServerConfig {
    QString id;
    QString name;
    QString serverUrl;
    int port;
    bool autoConnect;
};

class NatsPlugin : public PluginInterface
{
    Q_OBJECT
    Q_INTERFACES(PluginInterface)
    Q_PLUGIN_METADATA(IID "com.louhi.plugininterface/1.0" FILE "natsplugin.json")

public:
    NatsPlugin(QObject* parent = nullptr);
    ~NatsPlugin();

    PluginInfo getPluginInfo() const override;
    QVector<MenuEntry> getMenuEntries() const override;

    bool load() override;
    bool initialize() override;
    bool start() override;

    void publish(const QString& topic, const QString& payload) override;
    bool stop() override;
    bool unload() override;

    QWidget* getWidget() override;
    void configure(QWidget* parent) override;

    QJsonObject getConfig() const override;
    void setConfig(const QJsonObject& config) override;
    void setSubscribedTopics(const QStringList& topics) override;

private slots:
    void onClientConnected();
    void onClientDisconnected();

private:
    void buildStatusWidget();
    void updateStatusDisplay();
    void subscribeAllTopics(NatsClient* client);
    void connectToServer(const QString& serverId);
    void disconnectFromServer(const QString& serverId);

    NatsServerConfig configFromJson(const QJsonObject& obj) const;
    QJsonObject configToJson(const NatsServerConfig& config) const;

    PluginInfo m_info;
    QWidget* m_statusWidget;
    QVBoxLayout* m_mainLayout;
    QListWidget* m_serverStatusList;
    QList<NatsServerConfig> m_serverConfigs;
    QMap<QString, NatsClient*> m_clients;
    // class NatsClient; forward-declared above
    QStringList m_subscribedTopics;
    bool m_emconActive = false;
};

#endif
