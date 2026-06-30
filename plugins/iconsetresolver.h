#ifndef ICONSETRESOLVER_H
#define ICONSETRESOLVER_H

#include <QString>
#include <QMap>
#include <QImage>
#include <QHash>
#include <QDir>
#include <QSet>

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

    void loadAll(const QString& iconsetsDir, const QString& twoFiveTwoDir = QString());

    // Primary resolver: CoT type + optional milsymId → 2525B icon
    // Falls back to iconset only when iconsetPath is non-empty
    QImage resolveIcon(const QString& cotType,
                       const QString& iconsetPath = QString(),
                       const QString& milsymId = QString(),
                       const QString& callsign = QString(),
                       const QString& uid = QString()) const;
    // Find iconset index matching the given uid (from CoT <usericon iconsetpath>)
    int iconsetIndexByUid(const QString& uid) const;

private:
    void loadIconset(const QString& xmlPath);
    QImage loadIconImage(const QString& relativePath, const IconsetInfo& iconset) const;
    QImage findDefaultIcon(const QString& affiliation) const;
    static QString affiliationFromType(const QString& cotType);
    static QString typePrefix(const QString& cotType);

    // 2525B resolution
    QString cotToSidc(const QString& cotType) const;
    QString cleanSidc(const QString& sidc) const;
    QImage resolve2525Icon(const QString& sidc) const;

    QList<IconsetInfo> m_iconsets;
    mutable QHash<QString, QImage> m_imageCache;

    // 2525B icon directory and available file index (lowercase base names)
    QDir m_2525Dir;
    QSet<QString> m_2525Files; // e.g. "sfgp-----------"
};

#endif // ICONSETRESOLVER_H
