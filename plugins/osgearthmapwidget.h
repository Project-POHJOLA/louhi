#ifndef OSGEARTHMAPWIDGET_H
#define OSGEARTHMAPWIDGET_H

#include <QOpenGLWidget>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QTimer>
#include <QList>
#include <QMap>
#include <QImage>
#include <QDateTime>
#include <functional>
#include "mapsources.h"

#include <osg/ref_ptr>
#include <osgViewer/Viewer>
#include <osgEarth/MapNode>
#include <osgEarth/Map>
#include <osgEarth/AnnotationLayer>
#include <osgEarth/ScreenSpaceLayout>
#include <osgEarth/PlaceNode>
#include <osgEarth/RTTPicker>
#include <osgEarth/ObjectIndex>

struct MapEntity {
    QString uid;
    double lat = 0.0;
    double lon = 0.0;
    double alt = 0.0;
    QString callsign;
    QString cotType;
    QString iconsetPath;
    QString milsymId;   // from __milsym/__milicon/milsym/milicon detail
    QImage icon;
    QDateTime staleTime;
    QRgb colorArgb = 0;       // 0 = unset (no tint)
    QString detailXml;        // raw CoT <detail> XML for info widget
};

class OsgEarthMapWidget : public QOpenGLWidget
{
    Q_OBJECT

public:
    explicit OsgEarthMapWidget(QWidget* parent = nullptr);
    ~OsgEarthMapWidget();

    void setCenter(double lat, double lon);
    void setZoom(int zoom);

    void addOrUpdateEntity(const MapEntity& entity);
    void removeEntity(const QString& uid);
    void clearEntities();
    double latitude() const { return m_centerLat; }
    double longitude() const { return m_centerLon; }
    int zoom() const { return m_zoom; }
    int iconSize() const { return m_iconSize; }
    void setIconSize(int size);
    bool declutteringEnabled() const { return m_declutteringEnabled; }
    void setDeclutteringEnabled(bool enabled);

    void setSource(const MapSource& source);
    MapSource source() const { return m_currentSource; }

    QList<MapSource> customSources() const { return m_customSources; }
    void setCustomSources(const QList<MapSource>& sources);
    static QImage tintIcon(const QImage& icon, QRgb argb);

    // Called by the RTTPicker callback when an object is clicked
    void handleIconClick(const QString& uid) { emit entityClicked(uid); }

signals:
    void entityClicked(const QString& uid);
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
    void staleCheck();

    osg::ref_ptr<osgEarth::AnnotationLayer> m_annotationLayer;
    QMap<QString, osg::ref_ptr<osgEarth::PlaceNode>> m_entities;
    QMap<QString, QDateTime> m_staleTimes;
    QMap<QString, osg::ref_ptr<osg::Image>> m_cachedOsgIcons;
    QMap<QString, QImage> m_entityIcons;

    // ObjectID tracking for GPU-based picking
    QMap<osgEarth::ObjectID, QString> m_objectIdToEntity;
    osg::ref_ptr<osgEarth::Util::RTTPicker> m_picker;

    QTimer* m_staleTimer;
    osg::ref_ptr<osgEarth::Map> m_map;

    double m_centerLat;
    double m_centerLon;
    int m_zoom;
    bool m_declutteringEnabled = false;
    int m_iconSize = 32;

    QTimer* m_updateTimer;

    MapSource m_currentSource;
    QList<MapSource> m_customSources;
    bool m_mapInitialized;
};

#endif
