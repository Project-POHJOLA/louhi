#ifndef MAPPLUGIN_H
#define MAPPLUGIN_H

#include "../src/plugininterface.h"
#include <QJsonObject>
#include <QTimer>
#include <QtPlugin>

class MapWidget;

class MapPlugin : public PluginInterface
{
    Q_OBJECT
    Q_INTERFACES(PluginInterface)
    Q_PLUGIN_METADATA(IID "com.louhi.plugininterface/1.0" FILE "mapplugin.json")

public:
    MapPlugin(QObject* parent = nullptr);
    ~MapPlugin();

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

private:
    void applyConfigPosition();

    PluginInfo m_info;
    MapWidget* m_mapWidget;
    QTimer* m_locationTimeout;
    bool m_hasInitialPosition;

    double m_configLat;
    double m_configLon;
    int m_configZoom;
    QString m_configSourceName;
    QJsonObject m_storedConfig;

private slots:
    void onLocationTimeout();
};

#endif
