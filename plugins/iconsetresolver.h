#ifndef ICONSETRESOLVER_H
#define ICONSETRESOLVER_H

#include <QString>
#include <QMap>
#include <QImage>
#include <QHash>
#include <QDir>

struct IconsetInfo {
    QString name;
    QString uid;
    QString defaultFriendly;
    QString defaultHostile;
    QString defaultNeutral;
    QString defaultUnknown;
    QString defaultGroup;
    QDir baseDir;
    // type2525b -> relative path (groupName/iconName.png)
    QHash<QString, QString> typeMap;
};

class IconsetResolver
{
public:
    IconsetResolver();

    void loadAll(const QString& iconsetsDir);

    QImage resolveIcon(const QString& cotType, const QString& iconsetPath = QString()) const;

private:
    void loadIconset(const QString& xmlPath);
    QImage loadIconImage(const QString& relativePath, const IconsetInfo& iconset) const;
    QImage findDefaultIcon(const QString& affiliation) const;
    static QString affiliationFromType(const QString& cotType);
    static QString typePrefix(const QString& cotType);

    QList<IconsetInfo> m_iconsets;
    // Cache loaded images
    mutable QHash<QString, QImage> m_imageCache;
};

#endif // ICONSETRESOLVER_H
