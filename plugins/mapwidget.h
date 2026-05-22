#ifndef MAPWIDGET_H
#define MAPWIDGET_H

#include <QWidget>
#include <QNetworkAccessManager>
#include <QMap>
#include <QHash>
#include <QSet>
#include <QList>
#include <QPixmap>
#include <QPoint>
#include <QVariantMap>

struct MapSource {
    QString name;
    QString type;
    QString url;
    QString layers;
    QString format;
    QString crs;
    QString styles;
    int maxZoom;
    bool builtIn;

    MapSource()
        : format("image/png"), crs("EPSG:3857"), maxZoom(19), builtIn(false) {}
};

struct TileKey {
    int z, x, y;
    bool operator==(const TileKey& o) const {
        return z == o.z && x == o.x && y == o.y;
    }
    bool operator<(const TileKey& o) const {
        if (z != o.z) return z < o.z;
        if (x != o.x) return x < o.x;
        return y < o.y;
    }
};

inline uint qHash(const TileKey& key, uint seed = 0) {
    return qHash(QString("%1/%2/%3").arg(key.z).arg(key.x).arg(key.y), seed);
}

class MapWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MapWidget(QWidget* parent = nullptr);
    ~MapWidget();

    void setCenter(double lat, double lon);
    void setZoom(int zoom);
    double latitude() const { return m_centerLat; }
    double longitude() const { return m_centerLon; }
    int zoom() const { return m_zoom; }

    void setSource(const MapSource& source);
    MapSource source() const { return m_currentSource; }

    QList<MapSource> customSources() const { return m_customSources; }
    void setCustomSources(const QList<MapSource>& sources);
    void addSource(const MapSource& source);
    void removeSource(int index);

signals:
    void centerChanged(double lat, double lon);
    void zoomChanged(int zoom);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void requestTile(int z, int x, int y);
    void requestWmsTile(int z, int x, int y);
    QPixmap getCachedTile(int z, int x, int y);
    void ensureTiles();
    void pruneCache();

    QPointF latLonToWorld(double lat, double lon) const;
    double worldXToLon(double x) const;
    double worldYToLat(double y) const;

    static double mercatorProject(double lat);
    static double mercatorInverse(double y);

    double m_centerLat;
    double m_centerLon;
    int m_zoom;

    bool m_dragging;
    QPoint m_lastMousePos;

    MapSource m_currentSource;
    QList<MapSource> m_customSources;

    QNetworkAccessManager* m_network;

    QMap<TileKey, QPixmap> m_cache;
    QList<TileKey> m_cacheOrder;
    int m_maxCacheSize;

    QMap<TileKey, QNetworkReply*> m_pendingTileRequests;
};

#endif
