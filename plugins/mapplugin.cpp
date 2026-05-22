#include "mapplugin.h"
#include "mapwidget.h"
#include "mapsourcesdialog.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>

MapPlugin::MapPlugin(QObject* parent)
    : PluginInterface(parent)
    , m_mapWidget(nullptr)
    , m_locationTimeout(nullptr)
    , m_hasInitialPosition(false)
    , m_configLat(60.1699)
    , m_configLon(24.9384)
    , m_configZoom(10)
    , m_configSourceName("OSM Standard")
{
    m_info.id = "map_plugin";
    m_info.name = tr("Map");
    m_info.version = "0.1";
    m_info.description = tr("Map plugin with OSM, Carto Dark, and custom WMS/XYZ tile support");
    m_info.author = "LOUHI Team";
    m_info.type = PluginType::Map;
    m_info.enabled = true;
    m_info.dependencies = QStringList();
    m_info.capabilities = QStringList() << "Map" << "OSM" << "Carto Dark" << "WMS" << "XYZ";
    m_info.subscribeTopics = QStringList() << "location.position" << "location.position.reply";
    m_info.publishTopics = QStringList() << "location.request";
}

MapPlugin::~MapPlugin()
{
    unload();
}

PluginInfo MapPlugin::getPluginInfo() const
{
    return m_info;
}

QVector<MenuEntry> MapPlugin::getMenuEntries() const
{
    QVector<MenuEntry> entries;

    MenuEntry viewEntry;
    viewEntry.topMenu = tr("View");
    viewEntry.subMenus = QStringList() << tr("Show Map");
    entries.append(viewEntry);

    MenuEntry settingsEntry;
    settingsEntry.topMenu = tr("Settings");
    settingsEntry.subMenus = QStringList();
    entries.append(settingsEntry);

    return entries;
}

bool MapPlugin::load()
{
    qDebug() << "Map Plugin: Loading";
    return true;
}

bool MapPlugin::initialize()
{
    qDebug() << "Map Plugin: Initializing";

    m_mapWidget = new MapWidget();

    m_locationTimeout = new QTimer(this);
    m_locationTimeout->setSingleShot(true);
    m_locationTimeout->setInterval(5000);
    connect(m_locationTimeout, &QTimer::timeout, this, &MapPlugin::onLocationTimeout);

    connect(m_mapWidget, &MapWidget::centerChanged, this, [this](double lat, double lon) {
        qDebug() << "Map Plugin: Center changed to" << lat << "," << lon;
    });

    connect(m_mapWidget, &MapWidget::zoomChanged, this, [this](int zoom) {
        qDebug() << "Map Plugin: Zoom changed to" << zoom;
    });

    applyConfigPosition();

    return true;
}

void MapPlugin::applyConfigPosition()
{
    if (!m_mapWidget) return;

    if (m_storedConfig.contains("customSources")) {
        QJsonArray sourcesArray = m_storedConfig.value("customSources").toArray();
        QList<MapSource> sources;
        for (const QJsonValue& val : sourcesArray) {
            QJsonObject srcObj = val.toObject();
            MapSource src;
            src.name = srcObj.value("name").toString();
            src.type = srcObj.value("type").toString("xyz");
            src.url = srcObj.value("url").toString();
            src.layers = srcObj.value("layers").toString();
            src.format = srcObj.value("format").toString("image/png");
            src.crs = srcObj.value("crs").toString("EPSG:3857");
            src.styles = srcObj.value("styles").toString();
            src.maxZoom = srcObj.value("maxZoom").toInt(19);
            sources.append(src);
        }
        m_mapWidget->setCustomSources(sources);
    }

    if (m_configSourceName == "Carto Dark") {
        MapSource dark;
        dark.name = "Carto Dark";
        dark.type = "xyz";
        dark.url = "https://a.basemaps.cartocdn.com/dark_all/{z}/{x}/{y}.png";
        dark.maxZoom = 19;
        dark.builtIn = true;
        m_mapWidget->setSource(dark);
    } else {
        bool foundCustom = false;
        if (m_storedConfig.contains("customSources")) {
            QJsonArray sourcesArray = m_storedConfig.value("customSources").toArray();
            for (const QJsonValue& val : sourcesArray) {
                QJsonObject srcObj = val.toObject();
                if (srcObj.value("name").toString() == m_configSourceName) {
                    MapSource src;
                    src.name = srcObj.value("name").toString();
                    src.type = srcObj.value("type").toString("xyz");
                    src.url = srcObj.value("url").toString();
                    src.layers = srcObj.value("layers").toString();
                    src.format = srcObj.value("format").toString("image/png");
                    src.crs = srcObj.value("crs").toString("EPSG:3857");
                    src.styles = srcObj.value("styles").toString();
                    src.maxZoom = srcObj.value("maxZoom").toInt(19);
                    m_mapWidget->setSource(src);
                    foundCustom = true;
                }
            }
        }
        if (!foundCustom) {
            MapSource osm;
            osm.name = "OSM Standard";
            osm.type = "xyz";
            osm.url = "https://tile.openstreetmap.org/{z}/{x}/{y}.png";
            osm.maxZoom = 19;
            osm.builtIn = true;
            m_mapWidget->setSource(osm);
        }
    }

    if (m_hasInitialPosition) {
        m_mapWidget->setCenter(m_configLat, m_configLon);
        m_mapWidget->setZoom(m_configZoom);
    }
}

bool MapPlugin::start()
{
    qDebug() << "Map Plugin: Starting";

    if (!m_hasInitialPosition) {
        m_locationTimeout->start();
        emit messageReceived("location.request", "{}");
    }

    return true;
}

bool MapPlugin::stop()
{
    qDebug() << "Map Plugin: Stopping";
    if (m_locationTimeout) {
        m_locationTimeout->stop();
    }
    return true;
}

bool MapPlugin::unload()
{
    qDebug() << "Map Plugin: Unloading";
    if (m_locationTimeout) {
        m_locationTimeout->stop();
    }
    delete m_mapWidget;
    m_mapWidget = nullptr;
    m_hasInitialPosition = false;
    return true;
}

QWidget* MapPlugin::getWidget()
{
    return m_mapWidget;
}

void MapPlugin::configure(QWidget* parent)
{
    if (!m_mapWidget) return;

    MapSourcesDialog dialog(m_mapWidget->customSources(),
                            m_mapWidget->source().name,
                            parent);

    if (dialog.exec() == QDialog::Accepted) {
        m_mapWidget->setCustomSources(dialog.customSources());

        QString selectedName = dialog.selectedSourceName();
        if (selectedName == "OSM Standard") {
            MapSource osm;
            osm.name = "OSM Standard";
            osm.type = "xyz";
            osm.url = "https://tile.openstreetmap.org/{z}/{x}/{y}.png";
            osm.maxZoom = 19;
            osm.builtIn = true;
            m_mapWidget->setSource(osm);
        } else if (selectedName == "Carto Dark") {
            MapSource dark;
            dark.name = "Carto Dark";
            dark.type = "xyz";
            dark.url = "https://a.basemaps.cartocdn.com/dark_all/{z}/{x}/{y}.png";
            dark.maxZoom = 19;
            dark.builtIn = true;
            m_mapWidget->setSource(dark);
        } else {
            for (const MapSource& src : dialog.customSources()) {
                if (src.name == selectedName) {
                    m_mapWidget->setSource(src);
                    break;
                }
            }
        }
    }
}

QJsonObject MapPlugin::getConfig() const
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

void MapPlugin::setConfig(const QJsonObject& config)
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
        applyConfigPosition();
    }
}

void MapPlugin::deliverMessage(const QString& topic, const QString& payload)
{
    if (topic == "location.position" || topic == "location.position.reply") {
        QJsonDocument doc = QJsonDocument::fromJson(payload.toUtf8());
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            double lat = obj.value("latitude").toDouble();
            double lon = obj.value("longitude").toDouble();

            if (!m_hasInitialPosition && m_mapWidget) {
                m_hasInitialPosition = true;
                if (m_locationTimeout) {
                    m_locationTimeout->stop();
                }
                m_mapWidget->setCenter(lat, lon);
                qDebug() << "Map Plugin: Set initial position from location:" << lat << "," << lon;
            }
        }
    }
}

void MapPlugin::onLocationTimeout()
{
    if (!m_hasInitialPosition && m_mapWidget) {
        m_mapWidget->setCenter(60.1699, 24.9384);
        qDebug() << "Map Plugin: No location received, using default position";
    }
}
