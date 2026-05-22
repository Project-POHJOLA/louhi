#ifndef OSGEARTHMAPWIDGET_H
#define OSGEARTHMAPWIDGET_H

#include <QOpenGLWidget>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QTimer>
#include <QList>
#include "mapsources.h"

#include <osg/ref_ptr>
#include <osgViewer/Viewer>
#include <osgEarth/MapNode>
#include <osgEarth/Map>

class OsgEarthMapWidget : public QOpenGLWidget
{
    Q_OBJECT

public:
    explicit OsgEarthMapWidget(QWidget* parent = nullptr);
    ~OsgEarthMapWidget();

    void setCenter(double lat, double lon);
    void setZoom(int zoom);
    double latitude() const { return m_centerLat; }
    double longitude() const { return m_centerLon; }
    int zoom() const { return m_zoom; }

    void setSource(const MapSource& source);
    MapSource source() const { return m_currentSource; }

    QList<MapSource> customSources() const { return m_customSources; }
    void setCustomSources(const QList<MapSource>& sources);

signals:
    void centerChanged(double lat, double lon);
    void zoomChanged(int zoom);
    void sourceChanged(const QString& sourceName);

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    void setupMap();
    void updateCamera();
    void rebuildMapLayer();

    osg::ref_ptr<osgViewer::Viewer> m_viewer;
    osg::ref_ptr<osgEarth::MapNode> m_mapNode;
    osg::ref_ptr<osgEarth::Map> m_map;

    double m_centerLat;
    double m_centerLon;
    int m_zoom;

    QTimer* m_updateTimer;

    MapSource m_currentSource;
    QList<MapSource> m_customSources;
    bool m_mapInitialized;
};

#endif
