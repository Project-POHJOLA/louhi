#ifndef NATSPLUGIN_H
#define NATSPLUGIN_H

#include "../src/plugininterface.h"
#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <QtPlugin>

class QWidget;
class NatsClient;
class NatsSettingsDialog;

class NatsPlugin : public PluginInterface
{
    Q_OBJECT
    Q_INTERFACES(PluginInterface)

public:
    Q_PLUGIN_METADATA(IID "com.louhi.plugininterface/1.0" FILE "natsplugin.json")
    NatsPlugin(QObject* parent = nullptr);
    ~NatsPlugin();

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

    void setSubscribedTopics(const QStringList& topics);

private:
    PluginInfo m_info;
    NatsClient* m_natsClient;
    QWidget* m_statusWidget;

    QString m_serverUrl;
    int m_port;
    bool m_autoConnect;
    QStringList m_subscribedTopics;

    void updateStatusWidget();
};

#endif
