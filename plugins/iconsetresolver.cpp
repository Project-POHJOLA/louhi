#include "iconsetresolver.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QXmlStreamReader>

IconsetResolver::IconsetResolver()
{
}

void IconsetResolver::loadAll(const QString& iconsetsDir)
{
    QDir dir(iconsetsDir);
    if (!dir.exists()) {
        qWarning() << "IconsetResolver: directory does not exist:" << iconsetsDir;
        return;
    }

    // Find all iconset.xml files one level deep
    for (const QFileInfo& info : dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        QString xmlPath = info.absoluteFilePath() + "/iconset.xml";
        if (QFile::exists(xmlPath)) {
            loadIconset(xmlPath);
        }
    }

    qDebug() << "IconsetResolver: loaded" << m_iconsets.size() << "iconsets";
}

void IconsetResolver::loadIconset(const QString& xmlPath)
{
    QFile file(xmlPath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "IconsetResolver: cannot open" << xmlPath;
        return;
    }

    IconsetInfo info;
    info.baseDir = QFileInfo(xmlPath).absoluteDir();

    QXmlStreamReader xml(&file);
    while (!xml.atEnd() && !xml.hasError()) {
        xml.readNext();
        if (xml.isStartElement() && xml.name() == QStringLiteral("iconset")) {
            info.name = xml.attributes().value("name").toString();
            info.uid = xml.attributes().value("uid").toString();
            info.defaultFriendly = xml.attributes().value("defaultFriendly").toString();
            info.defaultHostile = xml.attributes().value("defaultHostile").toString();
            info.defaultNeutral = xml.attributes().value("defaultNeutral").toString();
            info.defaultUnknown = xml.attributes().value("defaultUnknown").toString();
            info.defaultGroup = xml.attributes().value("defaultGroup").toString();

            while (!xml.atEnd() && !xml.hasError()) {
                xml.readNext();
                if (xml.isStartElement() && xml.name() == QStringLiteral("icon")) {
                    QString type2525b = xml.attributes().value("type2525b").toString();
                    QString groupName = xml.attributes().value("groupName").toString();
                    QString iconName = xml.attributes().value("name").toString();

                    if (!type2525b.isEmpty() && !groupName.isEmpty() && !iconName.isEmpty()) {
                        info.typeMap[type2525b] = groupName + "/" + iconName;
                    }
                }
                if (xml.isEndElement() && xml.name() == QStringLiteral("iconset")) {
                    break;
                }
            }
            break;
        }
    }

    if (xml.hasError()) {
        qWarning() << "IconsetResolver: XML error in" << xmlPath << xml.errorString();
    }

    if (!info.uid.isEmpty()) {
        m_iconsets.append(info);
        qDebug() << "IconsetResolver: loaded" << info.name << "with" << info.typeMap.size() << "icons";
    }
}

QImage IconsetResolver::resolveIcon(const QString& cotType, const QString& iconsetPath) const
{
    // Priority 1: explicit iconsetpath from CoT detail
    if (!iconsetPath.isEmpty()) {
        // iconsetPath can be absolute or relative
        QFileInfo fi(iconsetPath);
        if (fi.isAbsolute() && fi.exists()) {
            QImage img(iconsetPath);
            if (!img.isNull()) return img;
        } else {
            // Search in all loaded iconsets
            for (const IconsetInfo& is : m_iconsets) {
                QString absPath = is.baseDir.absoluteFilePath(iconsetPath);
                if (QFile::exists(absPath)) {
                    QImage img(absPath);
                    if (!img.isNull()) return img;
                }
            }
        }
    }

    // Priority 2: exact CoT type match in typeMap
    for (const IconsetInfo& is : m_iconsets) {
        auto it = is.typeMap.constFind(cotType);
        if (it != is.typeMap.constEnd()) {
            QImage img = loadIconImage(it.value(), is);
            if (!img.isNull()) return img;
        }
    }

    // Priority 3: try type prefix (first 3 tokens, e.g. "a-f-G")
    for (const IconsetInfo& is : m_iconsets) {
        auto it = is.typeMap.constFind(typePrefix(cotType));
        if (it != is.typeMap.constEnd()) {
            QImage img = loadIconImage(it.value(), is);
            if (!img.isNull()) return img;
        }
    }

    // Priority 4: try type prefix (first 3 tokens + second 2 as "any")
    if (cotType.count('-') >= 2) {
        // Try the first 3 tokens with trailing wildcard equivalent
        QStringList parts = cotType.split('-');
        QString threeToken = parts.mid(0, 3).join('-');
        for (const IconsetInfo& is : m_iconsets) {
            // Try progressively shorter prefixes
            for (int i = 2; i >= 1; --i) {
                QString prefix = parts.mid(0, i).join('-');
                auto it = is.typeMap.constFind(prefix);
                if (it != is.typeMap.constEnd()) {
                    QImage img = loadIconImage(it.value(), is);
                    if (!img.isNull()) return img;
                }
            }
        }
    }

    // Priority 5: default icon based on affiliation
    return findDefaultIcon(affiliationFromType(cotType));
}

QImage IconsetResolver::loadIconImage(const QString& relativePath, const IconsetInfo& iconset) const
{
    // Check cache first
    auto it = m_imageCache.constFind(relativePath);
    if (it != m_imageCache.constEnd()) {
        return it.value();
    }

    QString absPath = iconset.baseDir.absoluteFilePath(relativePath);
    QImage img(absPath);
    if (!img.isNull()) {
        m_imageCache[relativePath] = img;
    }
    return img;
}

QImage IconsetResolver::findDefaultIcon(const QString& affiliation) const
{
    // Look for a default icon in any iconset that defines one for this affiliation
    // Prefer the Default iconset
    QString defaultName;
    for (const IconsetInfo& is : m_iconsets) {
        if (affiliation == "f") defaultName = is.defaultFriendly;
        else if (affiliation == "h") defaultName = is.defaultHostile;
        else if (affiliation == "n") defaultName = is.defaultNeutral;
        else defaultName = is.defaultUnknown;

        if (!defaultName.isEmpty()) {
            // The defaultName might be a bare filename like "salute.png" or have a group prefix
            // First try all subdirectories
            QDir baseDir = is.baseDir;
            for (const QFileInfo& subdir : baseDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
                QString candidate = subdir.absoluteFilePath() + QDir::separator() + defaultName;
                if (QFile::exists(candidate)) {
                    QImage img(candidate);
                    if (!img.isNull()) return img;
                }
            }
        }
    }
    return QImage();
}

QString IconsetResolver::affiliationFromType(const QString& cotType)
{
    // CoT type format: a-f-G-U-C, a-h-G, a-u-G, a-n-G, etc.
    // Second token = affiliation: f=friendly, h=hostile, u=unknown, n=neutral
    QStringList parts = cotType.split('-');
    if (parts.size() >= 2) {
        return parts[1];
    }
    return "u"; // default unknown
}

QString IconsetResolver::typePrefix(const QString& cotType)
{
    QStringList parts = cotType.split('-');
    if (parts.size() >= 3) {
        return parts.mid(0, 3).join('-');
    }
    return cotType;
}
