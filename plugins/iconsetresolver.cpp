#include "iconsetresolver.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QXmlStreamReader>
#include <QFile>

IconsetResolver::IconsetResolver()
{
}

void IconsetResolver::loadAll(const QString& iconsetsDir, const QString& twoFiveTwoDir)
{
    // ---- Load icon sets ----
    QDir dir(iconsetsDir);
    if (!dir.exists()) {
        qWarning() << "IconsetResolver: iconsets dir not found" << iconsetsDir;
    } else {
        for (const QFileInfo& info : dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            QString xmlPath = info.absoluteFilePath() + "/iconset.xml";
            if (QFile::exists(xmlPath)) {
                loadIconset(xmlPath);
            }
        }
        qDebug() << "IconsetResolver: loaded" << m_iconsets.size() << "iconsets";
    }

    // ---- Load 2525B icon directory ----
    if (!twoFiveTwoDir.isEmpty()) {
        m_2525Dir = QDir(twoFiveTwoDir);
        if (m_2525Dir.exists()) {
            const QStringList entries = m_2525Dir.entryList(QDir::Files | QDir::NoDotAndDotDot);
            for (const QString& fn : entries) {
                // Strip .png extension -> e.g. "sfgp-----------"
                QString base = fn;
                if (base.endsWith(".png", Qt::CaseInsensitive))
                    base.chop(4);
                m_2525Files.insert(base.toLower());
            }
            qDebug() << "IconsetResolver: loaded" << m_2525Files.size() << "2525B icons from" << twoFiveTwoDir;
        } else {
            qWarning() << "IconsetResolver: 2525B dir not found" << twoFiveTwoDir;
        }
    }
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

            int totalIcons = 0;
            int mappedIcons = 0;
            while (!xml.atEnd() && !xml.hasError()) {
                xml.readNext();
                if (xml.isStartElement() && xml.name() == QStringLiteral("icon")) {
                    totalIcons++;
                    QString type2525b = xml.attributes().value("type2525b").toString();
                    QString groupName = xml.attributes().value("groupName").toString();
                    QString iconName = xml.attributes().value("name").toString();

                    if (!type2525b.isEmpty() && !groupName.isEmpty() && !iconName.isEmpty()) {
                        info.typeMap[type2525b] = groupName + "/" + iconName;
                        mappedIcons++;
                    }
                }
                if (xml.isEndElement() && xml.name() == QStringLiteral("iconset")) {
                    break;
                }
            }
            qDebug() << "IconsetResolver:" << info.name << "-" << totalIcons << "icons," << mappedIcons << "type2525b mappings";
        }
    }

    if (xml.hasError()) {
        qWarning() << "IconsetResolver: XML error in" << xmlPath << xml.errorString();
    }

    if (!info.uid.isEmpty()) {
        m_iconsets.append(info);
        // (log already emitted per-iconset above with total/mapped counts)
    }
}

// ---- 2525B SIDC conversion ----

QString IconsetResolver::cotToSidc(const QString& cotType) const
{
    // Implements the same logic as tak-webview-cesium's cotToSidc()
    // CoT type format: a-f-G-U-C
    // Returns 15-char SIDC: S<affil><dim>P<func1><func2><func3><func4><func5><func6>---
    if (cotType.isEmpty())
        return QStringLiteral("sugp-----------");

    const QStringList parts = cotType.split('-');
    if (parts.size() < 3)
        return QStringLiteral("sugp-----------");

    // Position 1: Scheme (always S = Warfighting)
    QString sidc = QLatin1String("S");

    // Position 2: Affiliation from parts[1]
    QString affil = parts[1].toUpper();
    if (affil == QLatin1String("P")) {
        affil = QLatin1String("F"); // P (Pending) maps to F in sidc
    }
    if (affil.length() > 0) {
        sidc += affil.at(0);
    } else {
        sidc += QLatin1Char('U');
    }

    // Position 3: Battle Dimension from parts[2]
    QString dim = parts[2].toUpper();
    static const QSet<QString> validDims = {
        QLatin1String("P"), QLatin1String("A"), QLatin1String("G"),
        QLatin1String("S"), QLatin1String("U"), QLatin1String("F")
    };
    if (!validDims.contains(dim)) {
        dim = QLatin1String("G"); // default Ground
    }
    sidc += dim.at(0);

    // Position 4: Status (always P = Present)
    sidc += QLatin1Char('P');

    // Positions 5-10: Function code from parts[3..8]
    for (int i = 3; i <= 8; i++) {
        if (i < parts.size()) {
            const QString& part = parts[i];
            if (!part.isEmpty()) {
                sidc += part.at(0).toUpper();
            } else {
                sidc += QLatin1Char('-');
            }
        } else {
            sidc += QLatin1Char('-');
        }
    }

    // Pad to exactly 15 characters with '-'
    while (sidc.length() < 15)
        sidc += QLatin1Char('-');

    return sidc.left(15).toLower();
}

QString IconsetResolver::cleanSidc(const QString& sidc) const
{
    // Normalize a raw SIDC from milsym detail: uppercase, pad to 15, fill wildcards
    if (sidc.isEmpty())
        return QStringLiteral("sugp-----------");

    QString cleaned = sidc.toUpper();

    // Pad to 15
    while (cleaned.length() < 15)
        cleaned += QLatin1Char('-');
    if (cleaned.length() > 15)
        cleaned = cleaned.left(15);

    // Replace wildcards with sensible defaults
    for (int i = 0; i < cleaned.length(); i++) {
        if (cleaned[i] == QLatin1Char('*')) {
            switch (i) {
            case 0:  cleaned[i] = QLatin1Char('S'); break; // Scheme: Warfighting
            case 1:  cleaned[i] = QLatin1Char('U'); break; // Affiliation: Unknown
            case 2:  cleaned[i] = QLatin1Char('G'); break; // Dimension: Ground
            case 3:  cleaned[i] = QLatin1Char('P'); break; // Status: Present
            default: cleaned[i] = QLatin1Char('-'); break; // Modifiers: Null
            }
        }
    }

    // SOF (F) dimension check
    if (cleaned[2] == QLatin1Char('F') && cleaned[3] == QLatin1Char('-')) {
        cleaned[3] = QLatin1Char('P');
    }

    return cleaned.toLower();
}

QImage IconsetResolver::resolve2525Icon(const QString& sidc) const
{
    if (sidc.isEmpty() || m_2525Dir.path().isEmpty())
        return QImage();

    QString lower = sidc.toLower();

    // Try progressively shorter SIDC codes
    // Start with full 15 chars, then strip trailing non-dash characters
    for (int len = 15; len >= 4; --len) {
        QString candidate = lower.left(len);
        // Pad back to 15 with dashes for lookup
        while (candidate.length() < 15)
            candidate += QLatin1Char('-');

        if (m_2525Files.contains(candidate)) {
            // Check image cache
            auto it = m_imageCache.constFind(candidate);
            if (it != m_imageCache.constEnd())
                return it.value();

            // Load from disk
            QString absPath = m_2525Dir.absoluteFilePath(candidate + QLatin1String(".png"));
            QImage img(absPath);
            if (!img.isNull()) {
                m_imageCache[candidate] = img;
                return img;
            }
        }

        // For next iteration: strip the character at position (len-1)
        // by trimming one more from the effective function code portion
    }

    return QImage(); // Not found
}

// ---- Icon resolution ----

QImage IconsetResolver::resolveIcon(const QString& cotType,
                                     const QString& iconsetPath,
                                     const QString& milsymId) const
{
    // Priority 1: explicit usericon -> iconset resolver
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

    // Priority 2: explicit milsym/milicon SIDC -> 2525B directory
    if (!milsymId.isEmpty()) {
        QString sidc = cleanSidc(milsymId);
        QImage img = resolve2525Icon(sidc);
        if (!img.isNull()) return img;
    }

    // Priority 3: CoT type -> 2525B SIDC -> 2525B directory
    if (!cotType.isEmpty()) {
        QString sidc = cotToSidc(cotType);
        QImage img = resolve2525Icon(sidc);
        if (!img.isNull()) return img;
    }

    // Priority 4: exact CoT type match in iconset typeMap (fallback for non-2525B types)
    for (const IconsetInfo& is : m_iconsets) {
        auto it = is.typeMap.constFind(cotType);
        if (it != is.typeMap.constEnd()) {
            QImage img = loadIconImage(it.value(), is);
            if (!img.isNull()) return img;
        }
    }

    // Priority 5: try type prefix (first 3 tokens)
    for (const IconsetInfo& is : m_iconsets) {
        auto it = is.typeMap.constFind(typePrefix(cotType));
        if (it != is.typeMap.constEnd()) {
            QImage img = loadIconImage(it.value(), is);
            if (!img.isNull()) return img;
        }
    }

    // Priority 6: progressive prefix fallback
    if (cotType.count('-') >= 2) {
        QStringList parts = cotType.split('-');
        for (int i = 2; i >= 1; --i) {
            QString prefix = parts.mid(0, i).join('-');
            for (const IconsetInfo& is : m_iconsets) {
                auto it = is.typeMap.constFind(prefix);
                if (it != is.typeMap.constEnd()) {
                    QImage img = loadIconImage(it.value(), is);
                    if (!img.isNull()) return img;
                }
            }
        }
    }

    // Priority 7: default icon based on affiliation
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
