#include "osgearthmapwidget.h"
#include "tilecache.h"

#include <QDebug>
#include <QtMath>
#include <QSurfaceFormat>
#include <cstring>
#include <osg/GL>
#include <QTimer>
#include <osgUtil/LineSegmentIntersector>
#include <osgUtil/IntersectionVisitor>

#include <osg/Texture2D>
#include <osg/Geometry>
#include <osg/AutoTransform>
#include <osgEarth/GeoTransform>
#include <osg/Notify>
#include <osg/Geode>
#include <osg/ShapeDrawable>
#include <osg/Material>
#include <osgGA/StateSetManipulator>
#include <osgViewer/ViewerEventHandlers>

#include <osgEarth/Common>
#include <osgEarth/MapNode>
#include <osgEarth/Map>
#include <osgEarth/ImageLayer>
#include <osgEarth/ElevationLayer>
#include <osgEarth/TerrainOptions>
#include <osgEarth/Viewpoint>
#include <osgEarth/Profile>
#include <osgEarth/EarthManipulator>
#include <osgEarth/XYZ>
#include <osgEarth/IntersectionPicker>
#include <osgEarth/WMS>

#include <osgViewer/GraphicsWindow>
#include <osg/GraphicsContext>
#include <osgEarth/Viewpoint>
#include <osgEarth/EarthManipulator>
#include <osgEarth/XYZ>
#include <osgEarth/WMS>
#include <osgEarth/HTTPClient>
#include <osgEarth/GeoData>
#include <osgEarth/Cache>
#include <osgEarthDrivers/cache_filesystem/FileSystemCache>
OsgEarthMapWidget::OsgEarthMapWidget(QWidget* parent)
    : QOpenGLWidget(parent)
    , m_centerLat(60.1699)
    , m_centerLon(24.9384)
    , m_zoom(10)
    , m_staleTimer(nullptr)
    , m_updateTimer(nullptr)
    , m_mapInitialized(false)
{

    setMinimumSize(200, 200);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);

    QSurfaceFormat fmt;
    fmt.setDepthBufferSize(24);
    fmt.setStencilBufferSize(8);
    fmt.setSamples(4);
    fmt.setSwapInterval(0);
    fmt.setVersion(3, 3);
    fmt.setProfile(QSurfaceFormat::CoreProfile);
    setFormat(fmt);
}

OsgEarthMapWidget::~OsgEarthMapWidget()
{
    if (m_updateTimer) {
        m_updateTimer->stop();
    }
    if (m_mapInitialized) {
        makeCurrent();
        m_viewer = nullptr;
        m_mapNode = nullptr;
        m_map = nullptr;
        doneCurrent();
    }
}

void OsgEarthMapWidget::setCustomSources(const QList<MapSource>& sources)
{
    m_customSources = sources;
}

static osg::Image* qImageToOsgImage(const QImage& qimg, int size)
{
    QImage scaled = qimg.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    scaled = scaled.convertToFormat(QImage::Format_RGBA8888);
    // Flip vertically: QImage is top-to-bottom, OpenGL textures are bottom-to-top
    scaled = scaled.mirrored(false, true);
    unsigned char* data = new unsigned char[scaled.sizeInBytes()];
    memcpy(data, scaled.bits(), scaled.sizeInBytes());
    osg::Image* osgImg = new osg::Image();
    osgImg->setImage(scaled.width(), scaled.height(), 1, 4, GL_RGBA, GL_UNSIGNED_BYTE,
                     data, osg::Image::USE_NEW_DELETE);
    return osgImg;
}

QImage OsgEarthMapWidget::tintIcon(const QImage& icon, QRgb argb)
{
    if (icon.isNull())
        return icon;

    QImage tinted = icon.convertToFormat(QImage::Format_ARGB32);
    int tr = qRed(argb);
    int tg = qGreen(argb);
    int tb = qBlue(argb);

    for (int y = 0; y < tinted.height(); ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(tinted.scanLine(y));
        for (int x = 0; x < tinted.width(); ++x) {
            QRgb pixel = line[x];
            int r = (qRed(pixel) * tr) / 255;
            int g = (qGreen(pixel) * tg) / 255;
            int b = (qBlue(pixel) * tb) / 255;
            line[x] = qRgba(r, g, b, qAlpha(pixel));
        }
    }

    return tinted;
}
// Determine altitude mode from CoT type (third dash-separated token = dimension)
//   G=Ground → RELATIVE with 2m offset
//   A=Air    → ABSOLUTE (use reported HAE)
//   S=Sea    → RELATIVE with 0m
//   U=Subsurface → ABSOLUTE
static osgEarth::GeoPoint entityPosition(const MapEntity& entity)
{
    double alt = 2.0;
    osgEarth::AltitudeMode mode = osgEarth::ALTMODE_RELATIVE;

    QStringList parts = entity.cotType.split('-');
    if (parts.size() >= 3) {
        QString dim = parts[2].toUpper();
        // Air and Subsurface use absolute altitude, but clamp to RELATIVE if hae is near zero
        if (dim == QLatin1String("A") && qAbs(entity.alt) > 1.0) {
            mode = osgEarth::ALTMODE_ABSOLUTE;
            alt = entity.alt;
        } else if (dim == QLatin1String("U") && qAbs(entity.alt) > 1.0) {
            mode = osgEarth::ALTMODE_ABSOLUTE;
            alt = entity.alt;
        } else if (dim == QLatin1String("S")) {
            alt = 0.0;
        }
        // G, F, and anything with hae≈0 → RELATIVE at 2m
    }
    // Default: RELATIVE at 2m

    return osgEarth::GeoPoint(
        osgEarth::SpatialReference::get("wgs84"),
        entity.lon, entity.lat, alt, mode);
}

void OsgEarthMapWidget::addOrUpdateEntity(const MapEntity& entity)
{
    if (!m_mapInitialized || !m_entityRoot.valid())
        return;

    // Track stale time even for updates
    if (entity.staleTime.isValid())
        m_staleTimes[entity.uid] = entity.staleTime;

    osgEarth::GeoPoint pos = entityPosition(entity);

    // Detect icon change before overwriting m_entityIcons
    bool iconChanged = false;
    if (!entity.icon.isNull()) {
        auto prevIt = m_entityIcons.constFind(entity.uid);
        iconChanged = (prevIt == m_entityIcons.constEnd())
                      || (prevIt->cacheKey() != entity.icon.cacheKey());
        m_entityIcons[entity.uid] = entity.icon;
        if (iconChanged)
            m_cachedOsgIcons.remove(entity.uid);
    }

    // Check if entity already exists — update position and icon
    auto it = m_entities.find(entity.uid);
    if (it != m_entities.end()) {
        osgEarth::PlaceNode* node = it.value().get();
        node->setPosition(pos);
        // Reuse cached osg::Image if icon hasn't changed
        auto osgIt = m_cachedOsgIcons.constFind(entity.uid);
        if (osgIt != m_cachedOsgIcons.constEnd() && osgIt->valid())
            node->setIconImage(osgIt->get());
        return;
    }

    // Create new PlaceNode (no text label)
    osgEarth::PlaceNode* node = new osgEarth::PlaceNode();
    node->setPosition(pos);

    auto iconIt = m_entityIcons.constFind(entity.uid);
    if (iconIt != m_entityIcons.constEnd() && !iconIt->isNull()) {
        osg::Image* img = qImageToOsgImage(iconIt.value(), m_iconSize);
        m_cachedOsgIcons[entity.uid] = img;
        node->setIconImage(img);
    }
    m_entities[entity.uid] = node;
    // Tag the PlaceNode with an ObjectID for GPU-based RTTPicker picking
    osgEarth::ObjectID oid = osgEarth::Registry::objectIndex()->insert(node);
    osgEarth::Registry::objectIndex()->tagNode(node, oid);
    m_objectIdToEntity[oid] = entity.uid;
    m_entityRoot->addChild(node);
}

void OsgEarthMapWidget::clearEntities()
{
    for (auto it = m_entities.begin(); it != m_entities.end(); ++it) {
        m_entityRoot->removeChild(it.value().get());
    }
    m_entities.clear();
    m_objectIdToEntity.clear();
    m_staleTimes.clear();
    m_entityIcons.clear();
    m_cachedOsgIcons.clear();
}
void OsgEarthMapWidget::removeEntity(const QString& uid)
{
    m_cachedOsgIcons.remove(uid);
    m_staleTimes.remove(uid);
    m_entityIcons.remove(uid);

    // Remove ObjectID mapping for this uid
    for (auto oit = m_objectIdToEntity.begin(); oit != m_objectIdToEntity.end(); ) {
        if (oit.value() == uid)
            oit = m_objectIdToEntity.erase(oit);
        else
            ++oit;
    }

    auto it = m_entities.find(uid);
    if (it != m_entities.end()) {
        m_entityRoot->removeChild(it.value().get());
        m_entities.erase(it);
    }
}

void OsgEarthMapWidget::setIconSize(int size)
{
    m_iconSize = size;
    if (!m_mapInitialized) return;
    m_cachedOsgIcons.clear();
    // Rescale all existing entity icons to the new size
    for (auto it = m_entityIcons.begin(); it != m_entityIcons.end(); ++it) {
        auto entityIt = m_entities.constFind(it.key());
        if (entityIt != m_entities.constEnd() && entityIt.value().valid()) {
            osg::Image* img = qImageToOsgImage(it.value(), m_iconSize);
            m_cachedOsgIcons[it.key()] = img;
            entityIt.value()->setIconImage(img);
        }
    }
    update();
}

void OsgEarthMapWidget::setDeclutteringEnabled(bool enabled)
{
    m_declutteringEnabled = enabled;
    osgEarth::ScreenSpaceLayout::setDeclutteringEnabled(enabled);
    if (m_mapInitialized)
        update();
}
void OsgEarthMapWidget::staleCheck()
{
    if (m_staleTimes.isEmpty()) return;
    QDateTime now = QDateTime::currentDateTimeUtc();
    QStringList stale;
    for (auto it = m_staleTimes.begin(); it != m_staleTimes.end(); ++it) {
        if (it.value().isValid() && now >= it.value()) {
            stale.append(it.key());
        }
    }
    for (const QString& uid : stale) {
        removeEntity(uid);
    }
}

void OsgEarthMapWidget::initializeGL()
{
    if (m_mapInitialized) return;
    osgEarth::initialize();
    osgEarth::HTTPClient::setUserAgent("Louhi/0.1 (+https://github.com/sgofferj/LOUHI)");
    osg::DisplaySettings::instance()->setNumMultiSamples(4);

    m_viewer = new osgViewer::Viewer;

    osg::ref_ptr<osg::GraphicsContext::Traits> traits = new osg::GraphicsContext::Traits;
    traits->x = 0;
    traits->y = 0;
    traits->width = width();
    traits->height = height();
    traits->windowDecoration = false;
    traits->doubleBuffer = true;

    osg::ref_ptr<osgViewer::GraphicsWindowEmbedded> gw =
        new osgViewer::GraphicsWindowEmbedded(traits.get());

    // No-op swap callback: osgViewer::frame() must NOT swap buffers because
    // QOpenGLWidget manages the buffer swap/composite at the end of the paint
    // cycle. Without this, the viewer's internal swapBuffers() followed by Qt's
    // swap causes a visible black flash on every click/render.
    struct NoSwap : public osg::GraphicsContext::SwapCallback {
        void swapBuffersImplementation(osg::GraphicsContext*) override {}
    };
    gw->setSwapCallback(new NoSwap);
    m_viewer->getCamera()->setGraphicsContext(gw);
    m_viewer->getCamera()->setViewport(new osg::Viewport(0, 0, width(), height()));
    m_viewer->getCamera()->setProjectionMatrixAsPerspective(45.0, 1.0, 1.0, 10000000.0);
    m_viewer->getCamera()->setNearFarRatio(0.00001);
    // Don't clear color buffer on each frame — the terrain fills the whole
    // viewport, so clearing to black first just causes a visible flash when the
    // RTTPicker's pixel readback triggers a GPU flush mid-frame.
    m_viewer->getCamera()->setClearMask(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    m_viewer->setThreadingModel(osgViewer::Viewer::SingleThreaded);

    setupMap();

    osgEarth::Util::EarthManipulator* manip = new osgEarth::Util::EarthManipulator();
    manip->getSettings()->setMouseSensitivity(0.005);
    manip->getSettings()->setZoomToMouse(false);
    m_viewer->setCameraManipulator(manip);

    m_viewer->realize();

    m_updateTimer = new QTimer(this);
    connect(m_updateTimer, &QTimer::timeout, this, QOverload<>::of(&QWidget::update));
    m_updateTimer->start(16);

    m_mapInitialized = true;

    updateCamera();
}


// ── Custom picker that exposes protected pick()/setupRTT() methods ──
// Avoids EventRouter auto-registration from setView(), which passes raw Qt Y
// coordinates to pick() without flipping — breaking Y mapping vs the viewport.
// By calling pickAt() manually, we flip Y (Qt → OpenGL) before sampling the RTT.
class EntityIDPicker : public osgEarth::Util::ObjectIDPicker
{
public:
    void setupPicker(osgViewer::View* view, osg::Node* graph) {
        _view = view;
        if (graph) _graph = graph;
        setupRTT(view);
    }

    void pickAt(osg::View* view, float x, float y, ActionType action) {
        pick(view, x, y, action);
    }
};

void OsgEarthMapWidget::setupMap()
{
    m_map = new osgEarth::Map();

    // ── Set up osgEarth disk cache ──
    {
        QString cacheRoot = TileCache::cacheDirectory();
        osgEarth::Drivers::FileSystemCacheOptions cacheOpts;
        cacheOpts.rootPath() = cacheRoot.toStdString();
        osgEarth::Cache* cache = osgEarth::Util::CacheFactory::create(cacheOpts);
        if (cache) {
            m_map->setCache(cache);
            qDebug() << "OsgEarthMapWidget: disk cache at" << cacheRoot;
        } else {
            qWarning() << "OsgEarthMapWidget: failed to create disk cache at" << cacheRoot;
        }
    }

    rebuildMapLayer();

    m_mapNode = new osgEarth::MapNode(m_map);
    if (!m_mapNode) {
        qWarning() << "OsgEarthMapWidget: Failed to create MapNode";
        return;
    }


    // Create a world-space group for entity PlaceNodes.
    m_entityRoot = new osg::Group();
    m_entityRoot->setName("TacticalEntities");
    m_mapNode->addChild(m_entityRoot);

    // Start stale-check timer (every 5 seconds)
    m_staleTimer = new QTimer(this);
    connect(m_staleTimer, &QTimer::timeout, this, &OsgEarthMapWidget::staleCheck);
    m_staleTimer->start(5000);
    // ObjectIDPicker — GPU-based picking using ObjectID rendering to a small
    // offscreen texture. Uses EntityIDPicker subclass: no EventRouter registration,
    // pick is called manually from mouseReleaseEvent with Y-flipped coordinates.
    m_picker = new EntityIDPicker();
    m_picker->setRTTSize(512);   // higher RTT resolution for tighter pixel mapping
    m_picker->setBuffer(1);      // 1-pixel buffer (3×3 area in RTT space)
    m_picker->setupPicker(m_viewer, m_mapNode);
    m_picker->onPick([this](osgEarth::ObjectID id,
                            osgEarth::Util::ObjectIDPicker::ActionType type) {
        if (type == osgEarth::Util::ObjectIDPicker::ACTION_CLICK) {
            auto it = m_objectIdToEntity.constFind(id);
            if (it != m_objectIdToEntity.constEnd()) {
                qDebug() << "EntityIDPicker: hit" << it.value();
                handleIconClick(it.value());
            }
        }
    });
    m_mapNode->addChild(m_picker);
    // Apply declutter setting now that the map is initialized
    osgEarth::ScreenSpaceLayout::setDeclutteringEnabled(m_declutteringEnabled);
    m_viewer->setSceneData(m_mapNode);
}

void OsgEarthMapWidget::rebuildMapLayer()
{
    if (!m_map) return;

    // Remove existing image layers
    osgEarth::ImageLayerVector imageLayers;
    m_map->getLayers(imageLayers);
    for (auto& layer : imageLayers) {
        m_map->removeLayer(layer);
    }
    std::string layerName = m_currentSource.name.toStdString();
    std::string url = m_currentSource.url.toStdString();

    if (m_currentSource.type == "xyz" || m_currentSource.type == "tms") {
        osgEarth::XYZImageLayer* layer = new osgEarth::XYZImageLayer();
        layer->setName(layerName);
        layer->setURL(osgEarth::URI(url));
        layer->setFormat(m_currentSource.format.toStdString());
        layer->setProfile(osgEarth::Profile::create(
            osgEarth::Profile::SPHERICAL_MERCATOR));
        layer->options().maxLevel().setDefault(
            static_cast<unsigned>(m_currentSource.maxZoom));
        layer->options().minLevel().setDefault(0);
        layer->setCacheID(m_currentSource.name.toStdString());

        m_map->addLayer(layer);
        qDebug() << "OsgEarthMapWidget: Added XYZ layer" << m_currentSource.name;
    } else if (m_currentSource.type == "wms") {
        osgEarth::WMS::WMSImageLayerOptions wmsOpt;
        wmsOpt.url() = osgEarth::URI(url);
        wmsOpt.layers() = m_currentSource.layers.toStdString();
        wmsOpt.format() = m_currentSource.format.toStdString();
        wmsOpt.srs() = m_currentSource.crs.toStdString();
        wmsOpt.style() = m_currentSource.styles.toStdString();

        osgEarth::WMSImageLayer* layer = new osgEarth::WMSImageLayer(wmsOpt);
        layer->setName(layerName);
        layer->setCacheID(m_currentSource.name.toStdString());

        m_map->addLayer(layer);
        qDebug() << "OsgEarthMapWidget: Added WMS layer" << m_currentSource.name;
    } else {
        qWarning() << "OsgEarthMapWidget: Unknown source type" << m_currentSource.type;
    }
}

void OsgEarthMapWidget::paintGL()
{
    if (m_viewer.valid() && m_mapInitialized) {
        m_viewer->frame();

        osgEarth::Util::EarthManipulator* manip =
            dynamic_cast<osgEarth::Util::EarthManipulator*>(m_viewer->getCameraManipulator());
        if (manip) {
            const osgEarth::Viewpoint& vp = manip->getViewpoint();
            if (vp.focalPoint().isSet()) {
                m_centerLon = vp.focalPoint()->x();
                m_centerLat = vp.focalPoint()->y();
            }
            if (vp.range().isSet()) {
                double range = vp.range()->as(osgEarth::Units::METERS);
                m_zoom = static_cast<int>(qRound(1.0 + qLn(20000000.0 / range) / qLn(2.0)));
                m_zoom = qBound(1, m_zoom, 20);
            }
        }
    }
}

void OsgEarthMapWidget::resizeGL(int w, int h)
{
    if (m_viewer.valid()) {
        m_viewer->getCamera()->setViewport(0, 0, w, h);
        m_viewer->getCamera()->setProjectionMatrixAsPerspective(45.0, static_cast<double>(w)/h, 1.0, 10000000.0);
    }
}

void OsgEarthMapWidget::setCenter(double lat, double lon)
{
    m_centerLat = qBound(-90.0, lat, 90.0);
    m_centerLon = lon;
    while (m_centerLon > 180.0) m_centerLon -= 360.0;
    while (m_centerLon < -180.0) m_centerLon += 360.0;

    updateCamera();
    emit centerChanged(m_centerLat, m_centerLon);
}

void OsgEarthMapWidget::setZoom(int zoom)
{
    m_zoom = qBound(1, zoom, 20);
    updateCamera();
    emit zoomChanged(m_zoom);
}

void OsgEarthMapWidget::setSource(const MapSource& source)
{
    m_currentSource = source;

    if (m_map) {
        rebuildMapLayer();
        update();
    }

    emit sourceChanged(m_currentSource.name);
}

void OsgEarthMapWidget::updateCamera()
{
    if (!m_viewer.valid() || !m_mapInitialized) return;

    osgEarth::Util::EarthManipulator* manip =
        dynamic_cast<osgEarth::Util::EarthManipulator*>(m_viewer->getCameraManipulator());
    if (manip) {
        double range = 20000000.0 / qPow(2.0, m_zoom - 1);
        osgEarth::Viewpoint vp("home", m_centerLon, m_centerLat, 0.0, 0.0, -90.0, range);
        manip->setViewpoint(vp, 0.3);
    }

    update();
}

void OsgEarthMapWidget::mousePressEvent(QMouseEvent* event)
{
    setFocus();
    if (m_viewer.valid()) {
        osgGA::EventQueue* eq = m_viewer->getEventQueue();
        if (eq) {
            unsigned int button = 0;
            if (event->button() == Qt::LeftButton) button = 1;
            else if (event->button() == Qt::MiddleButton) button = 2;
            else if (event->button() == Qt::RightButton) button = 3;
            eq->mouseButtonPress(qRound(event->position().x()), qRound(event->position().y()), button);
        }
    }
    event->accept();
}

void OsgEarthMapWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (m_viewer.valid()) {
        osgGA::EventQueue* eq = m_viewer->getEventQueue();
        if (eq) {
            eq->mouseMotion(qRound(event->position().x()), qRound(event->position().y()));
        }
    }
    update();
    event->accept();
}

void OsgEarthMapWidget::mouseReleaseEvent(QMouseEvent* event)
{
    setFocus();
    if (m_viewer.valid()) {
        osgGA::EventQueue* eq = m_viewer->getEventQueue();
        if (eq) {
            unsigned int button = 0;
            if (event->button() == Qt::LeftButton) button = 1;
            else if (event->button() == Qt::MiddleButton) button = 2;
            else if (event->button() == Qt::RightButton) button = 3;
            eq->mouseButtonRelease(qRound(event->position().x()), qRound(event->position().y()), button);
        }

        // Manual GPU-based pick (EntityIDPicker) with Y flipped to Qt→OSG.
        // Do NOT flip in the event queue — the EarthManipulator needs Qt Y.
        // Only the RTT sampling in pick() needs OpenGL Y (bottom = 0).
        if (event->button() == Qt::LeftButton) {
            int flipY = height() - qRound(event->position().y());
            m_picker->pickAt(m_viewer, qRound(event->position().x()), flipY,
                             osgEarth::Util::ObjectIDPicker::ACTION_CLICK);
        }

        osgEarth::Util::EarthManipulator* manip =
            dynamic_cast<osgEarth::Util::EarthManipulator*>(m_viewer->getCameraManipulator());
        if (manip) {
            const osgEarth::Viewpoint& vp = manip->getViewpoint();
            if (vp.focalPoint().isSet()) {
                m_centerLon = vp.focalPoint()->x();
                m_centerLat = vp.focalPoint()->y();
            }
        }
        emit centerChanged(m_centerLat, m_centerLon);
    }
    update();
    event->accept();
}

void OsgEarthMapWidget::wheelEvent(QWheelEvent* event)
{
    if (m_viewer.valid()) {
        osgGA::EventQueue* eq = m_viewer->getEventQueue();
        if (eq) {
            if (event->angleDelta().y() > 0) {
                eq->mouseScroll(osgGA::GUIEventAdapter::SCROLL_DOWN);
            } else {
                eq->mouseScroll(osgGA::GUIEventAdapter::SCROLL_UP);
            }
        }
    }
    update();
    event->accept();
}
