#ifndef OSGEARTHPLUGIN_H
#define OSGEARTHPLUGIN_H

#include "plugininterface.h"
#include "mapsources.h"
#include "osgearthmapwidget.h"
#include "iconsetresolver.h"
#include <QJsonObject>
#include <QMap>
#include <QTimer>
#include <QtPlugin>

class BasemapDockWidget;

class OsgEarthPlugin : public PluginInterface
{
    Q_OBJECT
    Q_INTERFACES(PluginInterface)
    Q_PLUGIN_METADATA(IID "com.louhi.plugininterface/1.0" FILE "osgearthplugin.json")

public:
    OsgEarthPlugin(QObject* parent = nullptr);
    ~OsgEarthPlugin();

    PluginInfo getPluginInfo() const override;
    QVector<MenuEntry> getMenuEntries() const override;
    QVector<ToolbarEntry> getToolbarEntries() const override;
    void handleToolbarAction(const QString& actionId) override;

    bool load() override;
    bool initialize() override;
    bool start() override;
    bool stop() override;
    bool unload() override;

    QWidget* getWidget() override;
    QVector<QDockWidget*> getAdditionalDocks() override;
    void configure(QWidget* parent) override;

    QJsonObject getConfig() const override;
    void setConfig(const QJsonObject& config) override;

    void deliverMessage(const QString& topic, const QString& payload) override;

private:
    void applyConfig();
    MapSource configToMapSource(const QString& sourceName) const;
    QList<MapSource> currentCustomSources() const;
    void updateBasemapDockSources();

    MapEntity parseCotMessage(const QString& topic, const QString& payload);
    IconsetResolver m_iconResolver;

    PluginInfo m_info;
    OsgEarthMapWidget* m_mapWidget;
    BasemapDockWidget* m_basemapDock;
    bool m_hasInitialPosition;

    double m_configLat;
    double m_configLon;
    int m_configZoom;
    QString m_configSourceName;
    QJsonObject m_storedConfig;
};

#endif
