#include "osgearthmapwidget.h"
#include "tilecache.h"

#include <QDebug>
#include <QtMath>
#include <QSurfaceFormat>
#include <cstring>
#include <osg/GL>
#include <QTimer>

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
#include <osgEarth/WMS>

#include <osgViewer/GraphicsWindow>
#include <osg/GraphicsContext>
#include <osgEarth/Viewpoint>
#include <osgEarth/EarthManipulator>
#include <osgEarth/XYZ>
#include <osgEarth/WMS>
#include <osgEarth/HTTPClient>
#include <osgEarth/GeoData>
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

static osg::Image* qImageToOsgImage(const QImage& qimg, int size = 32)
{
    QImage scaled = qimg.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    scaled = scaled.convertToFormat(QImage::Format_RGBA8888);
    unsigned char* data = new unsigned char[scaled.sizeInBytes()];
    memcpy(data, scaled.bits(), scaled.sizeInBytes());
    osg::Image* osgImg = new osg::Image();
    osgImg->setImage(scaled.width(), scaled.height(), 1, 4, GL_RGBA, GL_UNSIGNED_BYTE,
                     data, osg::Image::USE_NEW_DELETE);
    return osgImg;
}
// Determine altitude mode from CoT type (third dash-separated token = dimension)
//   G=Ground → RELATIVE with 2m offset
//   A=Air    → ABSOLUTE (use reported HAE)
//   S=Sea    → RELATIVE with 0m
//   U=Subsurface → ABSOLUTE
//   F=SOF    → RELATIVE with 2m offset
//   default  → RELATIVE with 2m
static osgEarth::GeoPoint entityPosition(const MapEntity& entity)
{
    double alt = 2.0;
    osgEarth::AltitudeMode mode = osgEarth::ALTMODE_RELATIVE;

    QStringList parts = entity.cotType.split('-');
    if (parts.size() >= 3) {
        QString dim = parts[2].toUpper();
        if (dim == QLatin1String("A")) {
            mode = osgEarth::ALTMODE_ABSOLUTE;
            alt = entity.alt;
        } else if (dim == QLatin1String("U")) {
            mode = osgEarth::ALTMODE_ABSOLUTE;
            alt = entity.alt;
        } else if (dim == QLatin1String("S")) {
            alt = 0.0;
        }
    }
    // G, F, others: RELATIVE at 2m (default)

    return osgEarth::GeoPoint(
        osgEarth::SpatialReference::get("wgs84"),
        entity.lon, entity.lat, alt, mode);
}

void OsgEarthMapWidget::addOrUpdateEntity(const MapEntity& entity)
{
    if (!m_mapInitialized || !m_annotationLayer.valid())
        return;

    // Track stale time even for updates
    if (entity.staleTime.isValid())
        m_staleTimes[entity.uid] = entity.staleTime;

    osgEarth::GeoPoint pos = entityPosition(entity);

    // Check if entity already exists — update position and icon
    auto it = m_entities.find(entity.uid);
    if (it != m_entities.end()) {
        osgEarth::PlaceNode* node = it.value().get();
        node->setPosition(pos);
        if (!entity.icon.isNull()) {
            node->setIconImage(qImageToOsgImage(entity.icon));
        }
        node->setText(entity.callsign.toStdString());
        return;
    }

    // Create new PlaceNode
    osgEarth::PlaceNode* node = new osgEarth::PlaceNode();
    node->setPosition(pos);
    node->setText(entity.callsign.toStdString());

    if (!entity.icon.isNull()) {
        node->setIconImage(qImageToOsgImage(entity.icon));
    }

    m_annotationLayer->addChild(node);
    m_entities.insert(entity.uid, node);
}

void OsgEarthMapWidget::removeEntity(const QString& uid)
{
    m_staleTimes.remove(uid);
    auto it = m_entities.find(uid);
    if (it != m_entities.end()) {
        m_annotationLayer->getGroup()->removeChild(it.value().get());
        m_entities.erase(it);
    }
}

void OsgEarthMapWidget::clearEntities()
{
    for (auto it = m_entities.begin(); it != m_entities.end(); ++it) {
        m_annotationLayer->getGroup()->removeChild(it.value().get());
    }
    m_entities.clear();
    m_staleTimes.clear();
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

    m_viewer->getCamera()->setGraphicsContext(gw);
    m_viewer->getCamera()->setViewport(new osg::Viewport(0, 0, width(), height()));
    m_viewer->getCamera()->setProjectionMatrixAsPerspective(45.0, 1.0, 1.0, 10000000.0);
    m_viewer->getCamera()->setNearFarRatio(0.00001);

    m_viewer->setThreadingModel(osgViewer::Viewer::SingleThreaded);

    setupMap();

    osgEarth::Util::EarthManipulator* manip = new osgEarth::Util::EarthManipulator();
    manip->getSettings()->setMouseSensitivity(0.004);
    manip->getSettings()->setZoomToMouse(false);
    m_viewer->setCameraManipulator(manip);

    m_viewer->realize();

    m_updateTimer = new QTimer(this);
    connect(m_updateTimer, &QTimer::timeout, this, QOverload<>::of(&QWidget::update));
    m_updateTimer->start(16);

    m_mapInitialized = true;

    updateCamera();
}

void OsgEarthMapWidget::setupMap()
{
    m_map = new osgEarth::Map();

    rebuildMapLayer();

    m_mapNode = new osgEarth::MapNode(m_map);
    if (!m_mapNode) {
        qWarning() << "OsgEarthMapWidget: Failed to create MapNode";
        return;
    }


    // Create annotation layer for tactical entities
    m_annotationLayer = new osgEarth::AnnotationLayer();
    m_annotationLayer->setName("TacticalEntities");
    m_map->addLayer(m_annotationLayer);

    // Start stale-check timer (every 5 seconds)
    m_staleTimer = new QTimer(this);
    connect(m_staleTimer, &QTimer::timeout, this, &OsgEarthMapWidget::staleCheck);
    m_staleTimer->start(5000);
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
        layer->setCacheID(TileCache::cacheDirectory().toStdString() + "/" + m_currentSource.name.toStdString());

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
        layer->setCacheID(TileCache::cacheDirectory().toStdString() + "/" + m_currentSource.name.toStdString());

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
            eq->mouseButtonPress(event->x(), event->y(), button);
        }
    }
    event->accept();
}

void OsgEarthMapWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (m_viewer.valid()) {
        osgGA::EventQueue* eq = m_viewer->getEventQueue();
        if (eq) {
            eq->mouseMotion(event->x(), event->y());
        }
    }
    update();
    event->accept();
}

void OsgEarthMapWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_viewer.valid()) {
        osgGA::EventQueue* eq = m_viewer->getEventQueue();
        if (eq) {
            unsigned int button = 0;
            if (event->button() == Qt::LeftButton) button = 1;
            else if (event->button() == Qt::MiddleButton) button = 2;
            else if (event->button() == Qt::RightButton) button = 3;
            eq->mouseButtonRelease(event->x(), event->y(), button);
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
