#include "mapwidget.h"
#include "tilecache.h"
#include "version.h"
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QSslError>
#include <QDebug>
#include <QtMath>

static const double TILE_SIZE = 256.0;
static const double PI = 3.14159265358979323846;
static const double EARTH_RADIUS = 6378137.0;

MapWidget::MapWidget(QWidget* parent)
    : QWidget(parent)
    , m_centerLat(60.1699)
    , m_centerLon(24.9384)
    , m_zoom(10)
    , m_dragging(false)
    , m_network(new QNetworkAccessManager(this))
    , m_maxCacheSize(500)
{
    setMouseTracking(false);
    setMinimumSize(200, 200);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setAttribute(Qt::WA_NoSystemBackground, true);

    MapSource osm;
    osm.name = "OSM Standard";
    osm.type = "xyz";
    osm.url = "https://tile.openstreetmap.org/{z}/{x}/{y}.png";
    osm.maxZoom = 19;
    osm.builtIn = true;
    m_currentSource = osm;

    QTimer::singleShot(0, this, [this]() {
        ensureTiles();
    });


}

MapWidget::~MapWidget()
{
    QList<QNetworkReply*> replies;
    for (auto it = m_pendingTileRequests.begin(); it != m_pendingTileRequests.end(); ++it) {
        if (it.value()) {
            replies.append(it.value());
        }
    }
    m_pendingTileRequests.clear();
    for (QNetworkReply* reply : replies) {
        reply->abort();
        reply->deleteLater();
    }
}

void MapWidget::setCenter(double lat, double lon)
{
    m_centerLat = qBound(-85.0, lat, 85.0);
    m_centerLon = lon;
    while (m_centerLon > 180.0) m_centerLon -= 360.0;
    while (m_centerLon < -180.0) m_centerLon += 360.0;
    ensureTiles();
    update();
    emit centerChanged(m_centerLat, m_centerLon);
}

void MapWidget::setZoom(int zoom)
{
    m_zoom = qBound(1, zoom, m_currentSource.maxZoom);
    ensureTiles();
    update();
    emit zoomChanged(m_zoom);
}

void MapWidget::setSource(const MapSource& source)
{
    m_currentSource = source;
    m_cache.clear();
    m_cacheOrder.clear();

    for (auto it = m_pendingTileRequests.begin(); it != m_pendingTileRequests.end(); ++it) {
        if (it.value()) {
            it.value()->abort();
            it.value()->deleteLater();
        }
    }
    m_pendingTileRequests.clear();

    m_zoom = qMin(m_zoom, source.maxZoom);
    ensureTiles();
    update();
}

void MapWidget::setCustomSources(const QList<MapSource>& sources)
{
    m_customSources = sources;
}

void MapWidget::addSource(const MapSource& source)
{
    m_customSources.append(source);
}

void MapWidget::removeSource(int index)
{
    if (index >= 0 && index < m_customSources.size()) {
        QString removedName = m_customSources[index].name;
        m_customSources.removeAt(index);

        if (!m_currentSource.builtIn && m_currentSource.name == removedName) {
            MapSource osm;
            osm.name = "OSM Standard";
            osm.type = "xyz";
            osm.url = "https://tile.openstreetmap.org/{z}/{x}/{y}.png";
            osm.maxZoom = 19;
            osm.builtIn = true;
            setSource(osm);
        }
    }
}

double MapWidget::mercatorProject(double lat)
{
    double latRad = lat * PI / 180.0;
    return qLn(qTan(PI / 4.0 + latRad / 2.0));
}

double MapWidget::mercatorInverse(double y)
{
    return 180.0 / PI * (2.0 * qAtan(qExp(y)) - PI / 2.0);
}

QPointF MapWidget::latLonToWorld(double lat, double lon) const
{
    double n = qPow(2.0, m_zoom);
    double x = (lon + 180.0) / 360.0 * n * TILE_SIZE;
    double y = (1.0 - mercatorProject(lat) / PI) / 2.0 * n * TILE_SIZE;
    return QPointF(x, y);
}

double MapWidget::worldXToLon(double x) const
{
    double n = qPow(2.0, m_zoom);
    double lon = x / (n * TILE_SIZE) * 360.0 - 180.0;
    while (lon > 180.0) lon -= 360.0;
    while (lon < -180.0) lon += 360.0;
    return lon;
}

double MapWidget::worldYToLat(double y) const
{
    double n = qPow(2.0, m_zoom);
    double normalizedY = y / (n * TILE_SIZE);
    normalizedY = qBound(0.0, normalizedY, 1.0);
    return mercatorInverse(PI * (1.0 - 2.0 * normalizedY));
}

static int findFallbackLevel(const QMap<TileKey, QPixmap>& cache, int z, int x, int y)
{
    for (int fz = z - 1; fz >= 0; --fz) {
        int dz = z - fz;
        TileKey fk = { fz, x >> dz, y >> dz };
        if (cache.contains(fk))
            return fz;
    }
    return -1;
}

void MapWidget::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    QPointF centerWorld = latLonToWorld(m_centerLat, m_centerLon);
    double offsetX = width() / 2.0 - centerWorld.x();
    double offsetY = height() / 2.0 - centerWorld.y();

    int n = 1 << m_zoom;

    int minTx = static_cast<int>(qFloor(-offsetX / TILE_SIZE));
    int maxTx = static_cast<int>(qCeil((width() - offsetX) / TILE_SIZE));
    int minTy = static_cast<int>(qFloor(-offsetY / TILE_SIZE));
    int maxTy = static_cast<int>(qCeil((height() - offsetY) / TILE_SIZE));

    for (int tx = minTx; tx <= maxTx; ++tx) {
        for (int ty = minTy; ty <= maxTy; ++ty) {
            if (ty < 0 || ty >= n) continue;

            int wrappedTx = ((tx % n) + n) % n;
            QPointF tilePos(tx * TILE_SIZE + offsetX, ty * TILE_SIZE + offsetY);
            QRectF tileRect(tilePos, QSizeF(TILE_SIZE, TILE_SIZE));

            TileKey key = { m_zoom, wrappedTx, ty };
            auto it = m_cache.constFind(key);
            if (it != m_cache.constEnd()) {
                painter.drawPixmap(tilePos, it.value());
                continue;
            }

            int fz = findFallbackLevel(m_cache, m_zoom, wrappedTx, ty);
            if (fz >= 0) {
                int dz = m_zoom - fz;
                int div = 1 << dz;
                TileKey fk = { fz, wrappedTx >> dz, ty >> dz };
                QPixmap fb = m_cache.value(fk);
                QRectF src(
                    (wrappedTx % div) * TILE_SIZE / div,
                    (ty % div) * TILE_SIZE / div,
                    TILE_SIZE / div + 1, TILE_SIZE / div + 1);
                painter.drawPixmap(tileRect, fb, src);
            } else if (m_pendingTileRequests.empty()) {
                painter.fillRect(tileRect, QColor(220, 220, 220));
                painter.setPen(QColor(180, 180, 180));
                painter.drawText(tileRect, Qt::AlignCenter, "...");
            }
        }
    }
}

void MapWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_lastMousePos = event->pos();
        setCursor(Qt::ClosedHandCursor);
    }
}

void MapWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (!m_dragging) return;

    QPoint delta = event->pos() - m_lastMousePos;
    if (delta.x() == 0 && delta.y() == 0) return;

    QPointF centerWorld = latLonToWorld(m_centerLat, m_centerLon);
    double newWorldX = centerWorld.x() - delta.x();
    double newWorldY = centerWorld.y() - delta.y();

    m_centerLat = qBound(-85.0, worldYToLat(newWorldY), 85.0);
    m_centerLon = worldXToLon(newWorldX);

    m_lastMousePos = event->pos();
    ensureTiles();
    update();
}

void MapWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && m_dragging) {
        m_dragging = false;
        setCursor(Qt::ArrowCursor);
        emit centerChanged(m_centerLat, m_centerLon);
    }
}

void MapWidget::wheelEvent(QWheelEvent* event)
{
    double numDegrees = event->angleDelta().y() / 8.0;
    double numSteps = numDegrees / 15.0;

    int newZoom = m_zoom + (numSteps > 0 ? 1 : -1);
    newZoom = qBound(1, newZoom, m_currentSource.maxZoom);

    if (newZoom != m_zoom) {
        m_zoom = newZoom;
        ensureTiles();
        update();
        emit zoomChanged(m_zoom);
    }

    event->accept();
}

void MapWidget::resizeEvent(QResizeEvent*)
{
    ensureTiles();
}

void MapWidget::ensureTiles()
{
    if (width() < 1 || height() < 1) return;

    QPointF centerWorld = latLonToWorld(m_centerLat, m_centerLon);
    double offsetX = width() / 2.0 - centerWorld.x();
    double offsetY = height() / 2.0 - centerWorld.y();

    int n = 1 << m_zoom;

    int minTx = static_cast<int>(qFloor(-offsetX / TILE_SIZE)) - 1;
    int maxTx = static_cast<int>(qCeil((width() - offsetX) / TILE_SIZE)) + 1;
    int minTy = static_cast<int>(qFloor(-offsetY / TILE_SIZE)) - 1;
    int maxTy = static_cast<int>(qCeil((height() - offsetY) / TILE_SIZE)) + 1;

    QSet<TileKey> neededTiles;
    for (int tx = minTx; tx <= maxTx; ++tx) {
        for (int ty = minTy; ty <= maxTy; ++ty) {
            if (ty < 0 || ty >= n) continue;
            int wrappedTx = ((tx % n) + n) % n;
            TileKey key = { m_zoom, wrappedTx, ty };
            neededTiles.insert(key);

            if (!m_cache.contains(key) && !m_pendingTileRequests.contains(key)) {
                QByteArray cached = TileCache::loadTile(m_currentSource.name, m_zoom, wrappedTx, ty);
                if (!cached.isEmpty()) {
                    QPixmap pixmap;
                    if (pixmap.loadFromData(cached)) {
                        m_cache[key] = pixmap;
                        m_cacheOrder.removeOne(key);
                        m_cacheOrder.append(key);
                        continue;
                    }
                }
                if (m_currentSource.type == "xyz") {
                    requestTile(m_zoom, wrappedTx, ty);
                } else if (m_currentSource.type == "wms") {
                    requestWmsTile(m_zoom, wrappedTx, ty);
                }
            }
        }
    }

    QList<TileKey> toRemove;
    for (auto it = m_cache.begin(); it != m_cache.end(); ++it) {
        if (!neededTiles.contains(it.key())) {
            toRemove.append(it.key());
        }
    }
    for (const TileKey& key : toRemove) {
        m_cache.remove(key);
        m_cacheOrder.removeOne(key);
    }

    pruneCache();
}

void MapWidget::pruneCache()
{
    while (m_cache.size() > m_maxCacheSize && !m_cacheOrder.isEmpty()) {
        TileKey oldest = m_cacheOrder.takeFirst();
        m_cache.remove(oldest);
    }
}

void MapWidget::requestTile(int z, int x, int y)
{
    QString urlStr = m_currentSource.url;
    urlStr.replace("{z}", QString::number(z));
    urlStr.replace("{x}", QString::number(x));
    urlStr.replace("{y}", QString::number(y));

    QUrl qurl(urlStr);
    QNetworkRequest request(qurl);
    QString userAgent = QString("LOUHI-BMS/%1").arg(LOUHI_VERSION_STRING);
    request.setRawHeader("User-Agent", userAgent.toUtf8());
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);

    QNetworkReply* reply = m_network->get(request);
    TileKey key = { z, x, y };
    m_pendingTileRequests[key] = reply;

    connect(reply, &QNetworkReply::finished, this, [this, reply, key]() {
        m_pendingTileRequests.remove(key);

        if (reply->error() == QNetworkReply::NoError) {
            QByteArray data = reply->readAll();
            QPixmap pixmap;
            if (pixmap.loadFromData(data)) {
                m_cache[key] = pixmap;
                m_cacheOrder.removeOne(key);
                m_cacheOrder.append(key);
                TileCache::saveTile(m_currentSource.name, key.z, key.x, key.y, data);
                pruneCache();
                update();
            }
        }
        reply->deleteLater();
    });

    connect(reply, &QNetworkReply::sslErrors, this, [reply](const QList<QSslError>& errors) {
        Q_UNUSED(errors);
        reply->ignoreSslErrors();
    });
}

void MapWidget::requestWmsTile(int z, int x, int y)
{
    int n = 1 << z;

    double lonMin = (double)x / n * 360.0 - 180.0;
    double lonMax = (double)(x + 1) / n * 360.0 - 180.0;
    double latMin = mercatorInverse(PI * (1.0 - 2.0 * (y + 1) / n));
    double latMax = mercatorInverse(PI * (1.0 - 2.0 * y / n));

    QString bbox;
    if (m_currentSource.crs == "EPSG:3857") {
        double a = EARTH_RADIUS;
        double xMin = lonMin * a * PI / 180.0;
        double xMax = lonMax * a * PI / 180.0;
        double yMin = a * qLn(qTan(PI / 4.0 + latMin * PI / 360.0));
        double yMax = a * qLn(qTan(PI / 4.0 + latMax * PI / 360.0));
        bbox = QString("%1,%2,%3,%4")
            .arg(xMin, 0, 'f', 3)
            .arg(yMin, 0, 'f', 3)
            .arg(xMax, 0, 'f', 3)
            .arg(yMax, 0, 'f', 3);
    } else {
        bbox = QString("%1,%2,%3,%4")
            .arg(lonMin, 0, 'f', 6)
            .arg(latMin, 0, 'f', 6)
            .arg(lonMax, 0, 'f', 6)
            .arg(latMax, 0, 'f', 6);
    }

    QString urlStr = m_currentSource.url;
    urlStr.replace("{bbox}", bbox);
    urlStr.replace("{width}", QString::number((int)TILE_SIZE));
    urlStr.replace("{height}", QString::number((int)TILE_SIZE));
    urlStr.replace("{z}", QString::number(z));
    urlStr.replace("{x}", QString::number(x));
    urlStr.replace("{y}", QString::number(y));
    urlStr.replace("{layers}", m_currentSource.layers);
    urlStr.replace("{format}", m_currentSource.format);
    urlStr.replace("{crs}", m_currentSource.crs);
    urlStr.replace("{styles}", m_currentSource.styles);

    QUrl wmsUrl(urlStr);
    QNetworkRequest request(wmsUrl);
    QString userAgent = QString("LOUHI-BMS/%1").arg(LOUHI_VERSION_STRING);
    request.setRawHeader("User-Agent", userAgent.toUtf8());
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);

    QNetworkReply* reply = m_network->get(request);
    TileKey key = { z, x, y };
    m_pendingTileRequests[key] = reply;

    connect(reply, &QNetworkReply::finished, this, [this, reply, key]() {
        m_pendingTileRequests.remove(key);

        if (reply->error() == QNetworkReply::NoError) {
            QByteArray data = reply->readAll();
            QPixmap pixmap;
            if (pixmap.loadFromData(data)) {
                m_cache[key] = pixmap;
                m_cacheOrder.removeOne(key);
                m_cacheOrder.append(key);
                TileCache::saveTile(m_currentSource.name, key.z, key.x, key.y, data);
                pruneCache();
                update();
            }
        } else {
            qDebug() << "MapWidget: WMS tile error" << key.z << key.x << key.y
                     << reply->error() << reply->errorString();
        }
        reply->deleteLater();
    });

    connect(reply, &QNetworkReply::sslErrors, this, [reply](const QList<QSslError>& errors) {
        Q_UNUSED(errors);
        reply->ignoreSslErrors();
    });
}
