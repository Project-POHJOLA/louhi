#ifndef MAPSOURCES_H
#define MAPSOURCES_H

#include <QString>
#include <QList>
#include <QHash>

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

#endif
