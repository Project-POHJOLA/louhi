#include "osgearthplugin.h"
#include "osgearthmapwidget.h"
#include "osgearthbasemapdock.h"
#include "mapsourcesdialog.h"
#include "tilecache.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>

OsgEarthPlugin::OsgEarthPlugin(QObject* parent)
    : PluginInterface(parent)
    , m_mapWidget(nullptr)
    , m_basemapDock(nullptr)
    , m_hasInitialPosition(false)
    , m_configLat(60.1699)
    , m_configLon(24.9384)
    , m_configZoom(14)
    , m_configSourceName("OSM Standard")
{
    m_info.id = "osgearth_map";
    m_info.name = tr("OsgEarthMap");
    m_info.version = "0.1";
    m_info.description = tr("3D map plugin based on osgEarth with shared map sources");
    m_info.author = "LOUHI Team";
    m_info.type = PluginType::Map;
    m_info.enabled = true;
    m_info.dependencies = QStringList();
    m_info.capabilities = QStringList() << "Map" << "3D" << "osgEarth" << "OSM" << "WMS" << "XYZ";
    m_info.subscribeTopics = QStringList() << "location.position" << "location.position.reply";
    m_info.publishTopics = QStringList() << "location.request";
}

OsgEarthPlugin::~OsgEarthPlugin()
{
    unload();
}

PluginInfo OsgEarthPlugin::getPluginInfo() const
{
    return m_info;
}

QVector<MenuEntry> OsgEarthPlugin::getMenuEntries() const
{
    QVector<MenuEntry> entries;

    MenuEntry viewEntry;
    viewEntry.topMenu = tr("View");
    viewEntry.subMenus = QStringList() << tr("Show 3D Map");
    entries.append(viewEntry);

    MenuEntry basemapEntry;
    basemapEntry.topMenu = tr("Map");
    basemapEntry.subMenus = QStringList() << tr("Basemap");
    basemapEntry.addAsDirectAction = true;
    entries.append(basemapEntry);

    MenuEntry settingsEntry;
    settingsEntry.topMenu = tr("Settings");
    settingsEntry.subMenus = QStringList();
    entries.append(settingsEntry);

    return entries;
}

QVector<ToolbarEntry> OsgEarthPlugin::getToolbarEntries() const
{
    QVector<ToolbarEntry> entries;

    ToolbarEntry btn;
    btn.id = "osgearth_basemap";
    btn.text = tr("Basemap");
    btn.tooltip = tr("Select basemap for the globe");
    btn.group = tr("Map");
    entries.append(btn);

    return entries;
}

void OsgEarthPlugin::handleToolbarAction(const QString& actionId)
{
    if (actionId == "osgearth_basemap" && m_basemapDock) {
        m_basemapDock->show();
        m_basemapDock->raise();
    }
}

bool OsgEarthPlugin::load()
{
    qDebug() << "OsgEarth Plugin: Loading";
    return true;
}

bool OsgEarthPlugin::initialize()
{
    qDebug() << "OsgEarth Plugin: Initializing";

    m_mapWidget = new OsgEarthMapWidget();

    connect(m_mapWidget, &OsgEarthMapWidget::centerChanged, this, [this](double lat, double lon) {
        qDebug() << "OsgEarth Plugin: Center changed to" << lat << "," << lon;
    });

    connect(m_mapWidget, &OsgEarthMapWidget::zoomChanged, this, [this](int zoom) {
        qDebug() << "OsgEarth Plugin: Zoom changed to" << zoom;
    });

    m_basemapDock = new BasemapDockWidget();
    m_basemapDock->setObjectName("osgearthBasemapDock");

    connect(m_basemapDock, &BasemapDockWidget::sourceSelected, this,
        [this](const MapSource& source) {
            m_mapWidget->setSource(source);
        });

    connect(m_mapWidget, &OsgEarthMapWidget::sourceChanged, this,
        [this](const QString& sourceName) {
            m_basemapDock->setActiveSource(sourceName);
        });

    applyConfig();

    return true;
}

QList<MapSource> OsgEarthPlugin::currentCustomSources() const
{
    if (m_mapWidget)
        return m_mapWidget->customSources();
    return {};
}

void OsgEarthPlugin::updateBasemapDockSources()
{
    if (m_basemapDock)
        m_basemapDock->setSources(currentCustomSources());
}

MapSource OsgEarthPlugin::configToMapSource(const QString& sourceName) const
{
    if (sourceName == "OSM Standard") {
        MapSource osm;
        osm.name = "OSM Standard";
        osm.type = "xyz";
        osm.url = "https://tile.openstreetmap.org/{z}/{x}/{y}.png";
        osm.maxZoom = 19;
        osm.builtIn = true;
        return osm;
    }
    if (sourceName == "Carto Dark") {
        MapSource dark;
        dark.name = "Carto Dark";
        dark.type = "xyz";
        dark.url = "https://a.basemaps.cartocdn.com/dark_all/{z}/{x}/{y}.png";
        dark.maxZoom = 19;
        dark.builtIn = true;
        return dark;
    }
    if (m_storedConfig.contains("customSources")) {
        QJsonArray sourcesArray = m_storedConfig.value("customSources").toArray();
        for (const QJsonValue& val : sourcesArray) {
            QJsonObject srcObj = val.toObject();
            if (srcObj.value("name").toString() == sourceName) {
                MapSource src;
                src.name = srcObj.value("name").toString();
                src.type = srcObj.value("type").toString("xyz");
                src.url = srcObj.value("url").toString();
                src.layers = srcObj.value("layers").toString();
                src.format = srcObj.value("format").toString("image/png");
                src.crs = srcObj.value("crs").toString("EPSG:3857");
                src.styles = srcObj.value("styles").toString();
                src.maxZoom = srcObj.value("maxZoom").toInt(19);
                return src;
            }
        }
    }
    MapSource osm;
    osm.name = "OSM Standard";
    osm.type = "xyz";
    osm.url = "https://tile.openstreetmap.org/{z}/{x}/{y}.png";
    osm.maxZoom = 19;
    osm.builtIn = true;
    return osm;
}

void OsgEarthPlugin::applyConfig()
{
    if (!m_mapWidget) return;

    MapSource src = configToMapSource(m_configSourceName);
    m_mapWidget->setSource(src);

    if (m_storedConfig.contains("customSources")) {
        QJsonArray sourcesArray = m_storedConfig.value("customSources").toArray();
        QList<MapSource> sources;
        for (const QJsonValue& val : sourcesArray) {
            QJsonObject srcObj = val.toObject();
            MapSource s;
            s.name = srcObj.value("name").toString();
            s.type = srcObj.value("type").toString("xyz");
            s.url = srcObj.value("url").toString();
            s.layers = srcObj.value("layers").toString();
            s.format = srcObj.value("format").toString("image/png");
            s.crs = srcObj.value("crs").toString("EPSG:3857");
            s.styles = srcObj.value("styles").toString();
            s.maxZoom = srcObj.value("maxZoom").toInt(19);
            sources.append(s);
        }
        m_mapWidget->setCustomSources(sources);
    }

    updateBasemapDockSources();
    m_basemapDock->setActiveSource(m_configSourceName);

    if (m_hasInitialPosition) {
        m_mapWidget->setCenter(m_configLat, m_configLon);
    }
    m_mapWidget->setZoom(m_configZoom);
}

bool OsgEarthPlugin::start()
{
    qDebug() << "OsgEarth Plugin: Starting";
    return true;
}

bool OsgEarthPlugin::stop()
{
    qDebug() << "OsgEarth Plugin: Stopping";
    return true;
}

bool OsgEarthPlugin::unload()
{
    qDebug() << "OsgEarth Plugin: Unloading";
    delete m_basemapDock;
    m_basemapDock = nullptr;
    delete m_mapWidget;
    m_mapWidget = nullptr;
    m_hasInitialPosition = false;
    return true;
}

QWidget* OsgEarthPlugin::getWidget()
{
    return m_mapWidget;
}

QVector<QDockWidget*> OsgEarthPlugin::getAdditionalDocks()
{
    QVector<QDockWidget*> docks;
    if (m_basemapDock)
        docks.append(m_basemapDock);
    return docks;
}

void OsgEarthPlugin::configure(QWidget* parent)
{
    if (!m_mapWidget) return;

    MapSourcesDialog dialog(currentCustomSources(), parent);

    if (dialog.exec() == QDialog::Accepted) {
        m_mapWidget->setCustomSources(dialog.customSources());
        updateBasemapDockSources();
    }
}

QJsonObject OsgEarthPlugin::getConfig() const
{
    QJsonObject config;
    config["latitude"] = m_mapWidget ? m_mapWidget->latitude() : m_configLat;
    config["longitude"] = m_mapWidget ? m_mapWidget->longitude() : m_configLon;
    config["zoom"] = m_mapWidget ? m_mapWidget->zoom() : m_configZoom;
    config["sourceName"] = m_mapWidget ? m_mapWidget->source().name : m_configSourceName;

    QJsonArray sourcesArray;
    if (m_mapWidget) {
        for (const MapSource& src : m_mapWidget->customSources()) {
            QJsonObject srcObj;
            srcObj["name"] = src.name;
            srcObj["type"] = src.type;
            srcObj["url"] = src.url;
            srcObj["layers"] = src.layers;
            srcObj["format"] = src.format;
            srcObj["crs"] = src.crs;
            srcObj["styles"] = src.styles;
            srcObj["maxZoom"] = src.maxZoom;
            sourcesArray.append(srcObj);
        }
    }
    config["customSources"] = sourcesArray;

    return config;
}

void OsgEarthPlugin::setConfig(const QJsonObject& config)
{
    m_storedConfig = config;

    if (config.contains("latitude") && config.contains("longitude")) {
        m_configLat = config.value("latitude").toDouble(60.1699);
        m_configLon = config.value("longitude").toDouble(24.9384);
        m_configZoom = config.value("zoom").toInt(10);
        m_configSourceName = config.value("sourceName").toString("OSM Standard");
        m_hasInitialPosition = true;
    }

    if (m_mapWidget) {
        applyConfig();
    }
}

void OsgEarthPlugin::deliverMessage(const QString& topic, const QString& payload)
{
    Q_UNUSED(topic)
    Q_UNUSED(payload)
}
