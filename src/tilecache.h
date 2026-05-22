#ifndef TILECACHE_H
#define TILECACHE_H

#include <QString>
#include <QByteArray>
#include <QDir>

class TileCache
{
public:
    static QString cacheDirectory();
    static bool hasTile(const QString& sourceName, int z, int x, int y);
    static QByteArray loadTile(const QString& sourceName, int z, int x, int y);
    static bool saveTile(const QString& sourceName, int z, int x, int y, const QByteArray& data);
    static bool removeTile(const QString& sourceName, int z, int x, int y);
    static void clearCache(const QString& sourceName = QString());
    static qint64 cacheSize(const QString& sourceName = QString());

private:
    static QString tilePath(const QString& sourceName, int z, int x, int y);
    static QString sourceDir(const QString& sourceName);
};

#endif
