#include "osgearthplugin.h"
#include "osgearthmapwidget.h"
#include "osgearthbasemapdock.h"
#include "mapsourcesdialog.h"
#include "entityinfowidget.h"
#include "iconsetresolver.h"
#include <QXmlStreamReader>
#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QStandardPaths>
#include "tilecache.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QDialog>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QSpinBox>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QLabel>
#include <QCheckBox>

OsgEarthPlugin::OsgEarthPlugin(QObject* parent)
    : PluginInterface(parent)
    , m_mapWidget(nullptr)
    , m_basemapDock(nullptr)
    , m_hasInitialPosition(false)
    , m_entityInfo(nullptr)
    , m_configLat(60.1699)
    , m_configLon(24.9384)
    , m_configZoom(14)
    , m_configSourceName("OSM Standard")
    , m_iconSize(32)
    , m_declutteringEnabled(false)
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
    m_info.subscribeTopics = QStringList() << "location.position" << "location.position.reply" << "msg.>" << "alert.>" << "tak.>";
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

// Search order for the icons base directory (parent of map/iconsets/ and map/2525/)
static QString findIconsBaseDir()
{
    QStringList candidates = {
        // Build-tree layout: <appDir>/../assets/icons/
        QApplication::applicationDirPath() + "/../assets/icons",
        // Portable bundle layout: <appDir>/../icons/
        QApplication::applicationDirPath() + "/../icons",
    };

    // Platform-appropriate shared data locations via Qt
    const QStringList dataDirs = QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);
    for (const QString& dir : dataDirs) {
        candidates.append(dir + "/louhi/icons");
    }
    // Per-user writable override (last so user drops take priority)
    candidates.append(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/icons");

    for (const QString& dir : candidates) {
        QDir d(dir);
        if (d.exists("map/iconsets") && d.exists("map/2525")) {
            return d.absolutePath();
        }
    }

    qWarning() << "OsgEarthPlugin: icons directory not found — tried:";
    for (const QString& dir : candidates)
        qWarning() << "  " << dir;
    return QString();
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

    m_basemapDock = new BasemapDockWidget();
    m_basemapDock->setObjectName("osgearthBasemapDock");

    connect(m_basemapDock, &BasemapDockWidget::sourceSelected, this,
        [this](const MapSource& source) {
            m_mapWidget->setSource(source);
        });

    // Entity info widget — hidden until user clicks an icon
    m_entityInfo = new EntityInfoWidget();
    m_entityInfo->setObjectName("osgearthEntityInfo");

    connect(m_mapWidget, &OsgEarthMapWidget::entityClicked, this,
        [this](const QString& uid) {
            auto it = m_entityDetails.constFind(uid);
            if (it != m_entityDetails.constEnd()) {
                m_entityInfo->showEntity(it.value());
            }
        });

    // Load iconsets and 2525B tactical icons
    QString iconsDir = findIconsBaseDir();
    if (!iconsDir.isEmpty()) {
        m_iconResolver.loadAll(
            iconsDir + "/map/iconsets",
            iconsDir + "/map/2525");
    } else {
        qWarning() << "OsgEarthPlugin: icons unavailable, CoT icons will not render";
    }

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
    m_mapWidget->setIconSize(m_iconSize);
    m_mapWidget->setDeclutteringEnabled(m_declutteringEnabled);
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
    delete m_entityInfo;
    m_entityInfo = nullptr;
    delete m_basemapDock;
    m_basemapDock = nullptr;
    delete m_mapWidget;
    m_mapWidget = nullptr;
    m_entityDetails.clear();
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
    if (m_entityInfo)
        docks.append(m_entityInfo);
    return docks;
}

void OsgEarthPlugin::configure(QWidget* parent)
{
    if (!m_mapWidget) return;

    QDialog dialog(parent);
    dialog.setWindowTitle(tr("OsgEarth Map Settings"));
    QVBoxLayout* layout = new QVBoxLayout(&dialog);

    // Icon size
    QGroupBox* iconGroup = new QGroupBox(tr("Icon Display"), &dialog);
    QFormLayout* form = new QFormLayout(iconGroup);
    QSpinBox* iconSizeSpin = new QSpinBox(&dialog);
    iconSizeSpin->setRange(8, 128);
    iconSizeSpin->setValue(m_iconSize);
    form->addRow(tr("Icon size (px):"), iconSizeSpin);
    QCheckBox* declutterCheck = new QCheckBox(tr("Enable decluttering (fade/shrink overlapping icons)"), &dialog);
    declutterCheck->setChecked(m_declutteringEnabled);
    form->addRow(QString(), declutterCheck);
    layout->addWidget(iconGroup);

    // Map sources
    QGroupBox* sourcesGroup = new QGroupBox(tr("Custom Map Sources"), &dialog);
    QVBoxLayout* sourcesLayout = new QVBoxLayout(sourcesGroup);
    QLabel* infoLabel = new QLabel(
        tr("Manage custom tile sources. Built-in sources (OSM Standard, Carto Dark)\n"
           "are always available and can be selected from the basemap panel."));
    infoLabel->setWordWrap(true);
    sourcesLayout->addWidget(infoLabel);
    QPushButton* sourcesBtn = new QPushButton(tr("Manage Map Sources..."), &dialog);
    sourcesLayout->addWidget(sourcesBtn);
    layout->addWidget(sourcesGroup);

    QDialogButtonBox* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);

    QObject::connect(sourcesBtn, &QPushButton::clicked, [this]() {
        MapSourcesDialog dlg(currentCustomSources());
        if (dlg.exec() == QDialog::Accepted) {
            m_mapWidget->setCustomSources(dlg.customSources());
            updateBasemapDockSources();
        }
    });

    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        m_iconSize = iconSizeSpin->value();
        m_mapWidget->setIconSize(m_iconSize);
        m_declutteringEnabled = declutterCheck->isChecked();
        m_mapWidget->setDeclutteringEnabled(m_declutteringEnabled);
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
    config["declutteringEnabled"] = m_mapWidget ? m_mapWidget->declutteringEnabled() : m_declutteringEnabled;
    config["iconSize"] = m_mapWidget ? m_mapWidget->iconSize() : m_iconSize;

    return config;
}

void OsgEarthPlugin::setConfig(const QJsonObject& config)
{
    m_storedConfig = config;

    m_declutteringEnabled = config.value("declutteringEnabled").toBool(false);

    if (config.contains("latitude") && config.contains("longitude")) {
        m_configLat = config.value("latitude").toDouble(60.1699);
        m_configLon = config.value("longitude").toDouble(24.9384);
        m_configZoom = config.value("zoom").toInt(10);
        m_configSourceName = config.value("sourceName").toString("OSM Standard");
        m_hasInitialPosition = true;
        m_iconSize = config.value("iconSize").toInt(32);
        if (m_mapWidget)
            m_mapWidget->setIconSize(m_iconSize);
    }

    if (m_mapWidget) {
        applyConfig();
    }
}

MapEntity OsgEarthPlugin::parseCotMessage(const QString& topic, const QString& payload)
{
    MapEntity entity;
    Q_UNUSED(topic)

    // Verify it looks like CoT XML
    if (!payload.contains("<event") && !payload.contains("<Event"))
        return entity;

    QXmlStreamReader xml(payload);
    while (!xml.atEnd() && !xml.hasError()) {
        xml.readNext();
        if (xml.isStartElement() && xml.name() == QStringLiteral("event")) {
            entity.uid = xml.attributes().value("uid").toString();
            QString cotType = xml.attributes().value("type").toString();
            entity.cotType = cotType;
            QString staleStr = xml.attributes().value("stale").toString();

            // Parse stale time
            if (!staleStr.isEmpty()) {
                entity.staleTime = QDateTime::fromString(staleStr, Qt::ISODate);
                if (!entity.staleTime.isValid()) {
                    // Try with Z suffix
                    entity.staleTime = QDateTime::fromString(staleStr.replace("Z", ""), Qt::ISODate);
                }
            }

            // Only process atom (a-*) types and emergencies
            if (!cotType.startsWith("a-"))
                return MapEntity();

            // Skip ping/pong
            QString uidLower = entity.uid.toLower();
            if (uidLower.contains("ping") || uidLower.contains("pong") || uidLower.contains("takping"))
                return entity;

            // Parse point element
            while (!xml.atEnd() && !xml.hasError()) {
                xml.readNext();
                if (xml.isStartElement() && xml.name() == QStringLiteral("point")) {
                    entity.lat = xml.attributes().value("lat").toDouble();
                    entity.lon = xml.attributes().value("lon").toDouble();
                    entity.alt = xml.attributes().value("hae").toDouble();
                    break;
                }
            }

            // Parse detail element
            while (!xml.atEnd() && !xml.hasError()) {
                xml.readNext();
                if (xml.isStartElement() && xml.name() == QStringLiteral("contact")) {
                    entity.callsign = xml.attributes().value("callsign").toString();
                }
                if (xml.isStartElement() && xml.name() == QStringLiteral("usericon")) {
                    entity.iconsetPath = xml.attributes().value("iconsetpath").toString();
                }
                // Extract milsym/milicon 2525B symbol IDs
                if (xml.isStartElement() && (xml.name() == QStringLiteral("__milsym") ||
                    xml.name() == QStringLiteral("__milicon"))) {
                    QString id = xml.attributes().value("id").toString();
                    if (!id.isEmpty())
                        entity.milsymId = id;
                }
                // Parse color attribute (argb signed int, e.g. -65536 = red)
                if (xml.isStartElement() && xml.name() == QStringLiteral("color")) {
                    QString colorStr = xml.attributes().value("argb").toString();
                    if (!colorStr.isEmpty()) {
                        bool ok;
                        int argb = colorStr.toInt(&ok);
                        if (ok)
                            entity.colorArgb = static_cast<QRgb>(argb);
                    }
                }
                if (xml.isEndElement() && xml.name() == QStringLiteral("detail")) {
                }
            }

            break;
        }
    }

    if (xml.hasError()) {
        qDebug() << "OsgEarthPlugin: CoT parse error" << xml.errorString();
        return MapEntity();
}
    return entity;
}

void OsgEarthPlugin::deliverMessage(const QString& topic, const QString& payload)
{
    if (!m_mapWidget)
        return;

    // Try CoT parsing
    MapEntity entity = parseCotMessage(topic, payload);
    if (entity.uid.isEmpty())
        return;



    // Extract raw <detail> element XML for the info widget
    int detailStart = payload.indexOf(QStringLiteral("<detail"));
    int detailEnd = payload.indexOf(QStringLiteral("</detail>"), detailStart);
    if (detailStart >= 0 && detailEnd >= 0) {
        entity.detailXml = payload.mid(detailStart, detailEnd - detailStart + 9);
    }

    // Store entity data for the info widget (persists across icon updates)
    // Keep the existing detailXml if the new entity has none (update message
    // may omit detail)
    auto prevIt = m_entityDetails.constFind(entity.uid);
    if (prevIt != m_entityDetails.constEnd()) {
        MapEntity merged = entity;
        if (merged.detailXml.isEmpty())
            merged.detailXml = prevIt->detailXml;
        if (merged.callsign.isEmpty())
            merged.callsign = prevIt->callsign;
        if (merged.cotType.isEmpty())
            merged.cotType = prevIt->cotType;
        m_entityDetails[entity.uid] = merged;
    } else {
        m_entityDetails[entity.uid] = entity;
    }

    // Resolve icon: milsymId → 2525B, cotType → 2525B, iconsetPath → iconset
    entity.icon = m_iconResolver.resolveIcon(entity.cotType, entity.iconsetPath,
                                             entity.milsymId, entity.callsign, entity.uid);

    // Apply color tint if the CoT specifies a color
    if (entity.colorArgb != 0 && qAlpha(entity.colorArgb) > 0) {
        entity.icon = m_mapWidget->tintIcon(entity.icon, entity.colorArgb);
    }

    // Add or update entity on the map
    m_mapWidget->addOrUpdateEntity(entity);
}
