#include "tilecache.h"
#include <QStandardPaths>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDirIterator>
#include <QRegularExpression>
#include <QDebug>

QString TileCache::cacheDirectory()
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation);
    return base + "/Louhi/tiles";
}

QString TileCache::sourceDir(const QString& sourceName)
{
    QString safe = sourceName;
    safe.replace(QRegularExpression("[^a-zA-Z0-9_\\-]"), "_");
    return cacheDirectory() + "/" + safe;
}

QString TileCache::tilePath(const QString& sourceName, int z, int x, int y)
{
    return sourceDir(sourceName) + QString("/%1/%2/%3.png").arg(z).arg(x).arg(y);
}

bool TileCache::hasTile(const QString& sourceName, int z, int x, int y)
{
    return QFile::exists(tilePath(sourceName, z, x, y));
}

QByteArray TileCache::loadTile(const QString& sourceName, int z, int x, int y)
{
    QString path = tilePath(sourceName, z, x, y);
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return QByteArray();
    return file.readAll();
}

bool TileCache::saveTile(const QString& sourceName, int z, int x, int y, const QByteArray& data)
{
    QString path = tilePath(sourceName, z, x, y);
    QDir dir = QFileInfo(path).dir();
    if (!dir.exists())
        dir.mkpath(".");
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    return file.write(data) == data.size();
}

bool TileCache::removeTile(const QString& sourceName, int z, int x, int y)
{
    return QFile::remove(tilePath(sourceName, z, x, y));
}

void TileCache::clearCache(const QString& sourceName)
{
    QString path = sourceName.isEmpty() ? cacheDirectory() : sourceDir(sourceName);
    QDir dir(path);
    if (dir.exists())
        dir.removeRecursively();
}

qint64 TileCache::cacheSize(const QString& sourceName)
{
    QString path = sourceName.isEmpty() ? cacheDirectory() : sourceDir(sourceName);
    QDir dir(path);
    if (!dir.exists())
        return 0;
    qint64 size = 0;
    QDirIterator it(path, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        size += it.fileInfo().size();
    }
    return size;
}
