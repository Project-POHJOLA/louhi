#include "osgearthmapwidget.h"
#include "tilecache.h"

#include <QDebug>
#include <QtMath>
#include <QSurfaceFormat>
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

OsgEarthMapWidget::OsgEarthMapWidget(QWidget* parent)
    : QOpenGLWidget(parent)
    , m_centerLat(60.1699)
    , m_centerLon(24.9384)
    , m_zoom(10)
    , m_updateTimer(nullptr)
    , m_mapInitialized(false)
{
    osg::setNotifyLevel(osg::WARN);

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

void OsgEarthMapWidget::initializeGL()
{
    if (m_mapInitialized) return;
    osgEarth::initialize();
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
