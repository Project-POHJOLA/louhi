#include "entityinfowidget.h"

#include <QVBoxLayout>
#include <QXmlStreamReader>
#include <QtMath>
#include <QRegularExpression>
#include <QLocale>
#include <cctype>

EntityInfoWidget::EntityInfoWidget(QWidget* parent)
    : QDockWidget(parent)
    , m_browser(new QTextBrowser(this))
{
    setWindowTitle(tr("Entity Info"));
    setObjectName("entityInfoDock");
    setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);

    m_browser->setOpenExternalLinks(true);
    m_browser->setReadOnly(true);
    m_browser->setFrameShape(QFrame::NoFrame);
    m_browser->document()->setDefaultStyleSheet(QStringLiteral(
        "body { font-family: sans-serif; padding: 8px; }"
        "h3 { color: #4af; margin: 0 0 4px; }"
        ".label { color: #888; font-size: small; }"
        ".val { font-weight: bold; }"
        ".emergency { color: red; font-weight: bold; }"
        ".hashtag { color: #4af; cursor: pointer; }"
        ".section { margin-top: 10px; }"
    ));

    setWidget(m_browser);

    // Hidden by default; shown on first entity click
    hide();
}

void EntityInfoWidget::showEntity(const MapEntity& entity)
{
    m_browser->setHtml(formatHtml(entity));
    show();
    raise();
}

void EntityInfoWidget::clear()
{
    m_browser->clear();
    hide();
}

// ── HTML formatting ──

QString EntityInfoWidget::formatHtml(const MapEntity& entity) const
{
    QString html;
    html += QStringLiteral("<body>");

    // Header: callsign + CoT type
    QString displayName = entity.callsign.isEmpty() ? entity.uid : entity.callsign;
    html += QStringLiteral("<h3>%1</h3>").arg(displayName.toHtmlEscaped());
    html += QStringLiteral("<div class=\"label\">%1</div>")
                .arg(entity.cotType.toHtmlEscaped());

    // 2525B SIDC if available
    if (!entity.milsymId.isEmpty()) {
        html += QStringLiteral("<div class=\"label\">%1</div>")
                    .arg(entity.milsymId.toHtmlEscaped());
    }

    html += QStringLiteral("<hr/>");

    // Position
    html += formatPosition(entity.lat, entity.lon);

    // Altitude
    html += QStringLiteral("<div><span class=\"label\">%1:</span> <span class=\"val\">%2</span></div>")
                .arg(tr("Alt"), QString::number(entity.alt, 'f', 1) + QStringLiteral("m"));

    // ── Parse detail XML ──
    QString detailXml = entity.detailXml;

    // Track/Course
    QString courseStr = parseAttr(detailXml, "track", "course");
    QString speedStr = parseAttr(detailXml, "track", "speed");
    if (!courseStr.isEmpty() || !speedStr.isEmpty()) {
        html += QStringLiteral("<div class=\"section\"><b>%1:</b><br/>").arg(tr("Movement"));
        if (!courseStr.isEmpty())
            html += QStringLiteral("<span class=\"label\">%1:</span> %2&deg;<br/>")
                        .arg(tr("Course"), courseStr);
        if (!speedStr.isEmpty()) {
            double speedMs = speedStr.toDouble();
            html += QStringLiteral("<span class=\"label\">%1:</span> %2")
                        .arg(tr("Speed"), formatSpeedKmph(speedMs).toHtmlEscaped());
        }
        html += QStringLiteral("</div>");
    }

    // Emergency / squawk
    QString squawk = parseDetail(detailXml, "_squawk");
    if (squawk.isEmpty())
        squawk = parseDetail(detailXml, "emergency");
    if (!squawk.isEmpty()) {
        html += QStringLiteral("<div class=\"section emergency\">%1: %2</div>")
                    .arg(tr("Emergency"), squawk.toHtmlEscaped());
    }

    // Contact info
    QString xmpp = parseDetail(detailXml, "xmpp");
    QString mail = parseDetail(detailXml, "email");
    QString phone = parseDetail(detailXml, "phone");
    if (!xmpp.isEmpty() || !mail.isEmpty() || !phone.isEmpty()) {
        html += QStringLiteral("<div class=\"section\"><b>%1:</b><br/>").arg(tr("Contact"));
        if (!xmpp.isEmpty())
            html += QStringLiteral("XMPP: <a href=\"xmpp:%1\" style=\"color:#4af;\">%1</a><br/>")
                        .arg(xmpp.toHtmlEscaped());
        if (!mail.isEmpty())
            html += QStringLiteral("Email: <a href=\"mailto:%1\" style=\"color:#4af;\">%1</a><br/>")
                        .arg(mail.toHtmlEscaped());
        if (!phone.isEmpty())
            html += QStringLiteral("Phone: <a href=\"tel:%1\" style=\"color:#4af;\">%1</a><br/>")
                        .arg(phone.toHtmlEscaped());
        html += QStringLiteral("</div>");
    }

    // Battery
    QString batteryStr = parseAttr(detailXml, "status", "battery");
    if (batteryStr.isEmpty())
        batteryStr = parseDetail(detailXml, "battery");
    if (!batteryStr.isEmpty()) {
        bool ok;
        int battery = batteryStr.toInt(&ok);
        if (ok)
            html += formatBatteryBar(battery);
    }

    // Remarks
    QString remarks = parseDetail(detailXml, "remarks");
    if (!remarks.isEmpty()) {
        html += QStringLiteral("<div class=\"section\"><b>%1:</b><br/>%2</div>")
                    .arg(tr("Remarks"), formatRemarks(remarks));
    }

    // Event link (GDACS, AIS, ADSB)
    QString linkUrl = parseDetail(detailXml, "__link_url");
    if (linkUrl.isEmpty())
        linkUrl = parseAttr(detailXml, "link", "url");
    if (!linkUrl.isEmpty()) {
        QString linkLabel = linkLabelForUid(entity.uid);
        html += QStringLiteral("<div class=\"section\"><b>%1:</b><br/>")
                    .arg(tr("Event Link"));
        html += QStringLiteral("<a href=\"%1\" style=\"color:#4af;\">%2</a>")
                    .arg(linkUrl.toHtmlEscaped(), linkLabel.toHtmlEscaped());
        html += QStringLiteral("</div>");
    }

    // Group info
    QString groupName = parseAttr(detailXml, "group", "name");
    QString groupRole = parseAttr(detailXml, "group", "role");
    if (!groupName.isEmpty() || !groupRole.isEmpty()) {
        html += QStringLiteral("<div class=\"section\"><b>%1:</b><br/>").arg(tr("Group"));
        if (!groupName.isEmpty())
            html += QStringLiteral("%1%2").arg(tr("Name: "), groupName.toHtmlEscaped());
        if (!groupRole.isEmpty())
            html += QStringLiteral(" (%1)").arg(groupRole.toHtmlEscaped());
        html += QStringLiteral("</div>");
    }

    // Stale time
    if (entity.staleTime.isValid()) {
        html += QStringLiteral("<div class=\"section label\">%1: %2</div>")
                    .arg(tr("Stale"),
                         entity.staleTime.toLocalTime().toString(Qt::ISODate));
    }

    // UID (always shown for debugging/tracking)
    html += QStringLiteral("<div class=\"section label\">%1: %2</div>")
                .arg(tr("UID"), entity.uid.toHtmlEscaped());

    html += QStringLiteral("</body>");
    return html;
}

QString EntityInfoWidget::formatPosition(double lat, double lon) const
{
    QString html;
    html += QStringLiteral("<div><span class=\"label\">%1:</span> "
                           "<span class=\"val\">%2, %3</span></div>")
                .arg(tr("Pos"),
                     QString::number(lat, 'f', 5),
                     QString::number(lon, 'f', 5));

    // MGRS
    QString mgrs = mgrsFromLatLon(lat, lon);
    if (!mgrs.isEmpty()) {
        html += QStringLiteral("<div><span class=\"label\">%1:</span> "
                               "<span class=\"val\">%2</span></div>")
                    .arg(tr("MGRS"), mgrs.toHtmlEscaped());
    }
    return html;
}

QString EntityInfoWidget::mgrsFromLatLon(double lat, double lon) const
{
    // Basic MGRS conversion: UTM zone + latitude band + easting/northing
    // Valid range: -80 to +84 degrees latitude (polar regions use UPS)

    if (lat < -80.0 || lat > 84.0)
        return QString();

    int zone = static_cast<int>(qFloor((lon + 180.0) / 6.0)) + 1;
    if (zone > 60) zone = 60;

    // Latitude band letters: C-X (excluding I, O)
    static const char* bands = "CDEFGHJKLMNPQRSTUVWXX";
    int bandIdx = qBound(0, static_cast<int>(qFloor((lat + 80.0) / 8.0)), 19);
    char bandChar = bands[bandIdx];

    // UTM central meridian
    double cm = (zone - 1) * 6.0 - 180.0 + 3.0;

    // Transverse Mercator projection
    double eccSq = 0.00669437999014;    // WGS84 eccentricity squared
    double a = 6378137.0;               // WGS84 semi-major axis
    double k0 = 0.9996;

    double latRad = qDegreesToRadians(lat);
    double lonRad = qDegreesToRadians(lon);
    double cmRad = qDegreesToRadians(cm);

    double sinLat = qSin(latRad);
    double cosLat = qCos(latRad);
    double tanLat = qTan(latRad);
    double sin2Lat = sinLat * sinLat;

    double N = a / qSqrt(1.0 - eccSq * sin2Lat);
    double T = tanLat * tanLat;
    double C = eccSq * cosLat * cosLat / (1.0 - eccSq);
    double A = cosLat * (lonRad - cmRad);

    double M = a * ((1.0 - eccSq / 4.0 - 3.0 * eccSq * eccSq / 64.0
                     - 5.0 * eccSq * eccSq * eccSq / 256.0) * latRad
                    - (3.0 * eccSq / 8.0 + 3.0 * eccSq * eccSq / 32.0
                       + 45.0 * eccSq * eccSq * eccSq / 1024.0) * qSin(2.0 * latRad)
                    + (15.0 * eccSq * eccSq / 256.0
                       + 45.0 * eccSq * eccSq * eccSq / 1024.0) * qSin(4.0 * latRad)
                    - (35.0 * eccSq * eccSq * eccSq / 3072.0) * qSin(6.0 * latRad));

    double easting = k0 * N * (A + (1.0 - T + C) * A * A * A / 6.0
                               + (5.0 - 18.0 * T + T * T + 72.0 * C
                                  - 58.0 * eccSq) * A * A * A * A * A / 120.0);
    easting += 500000.0;   // false easting

    double northing = k0 * (M + N * tanLat * (A * A / 2.0
                                              + (5.0 - T + 9.0 * C + 4.0 * C * C) * A * A * A * A / 24.0
                                              + (61.0 - 58.0 * T + T * T + 600.0 * C
                                                 - 330.0 * eccSq) * A * A * A * A * A * A / 720.0));
    if (lat < 0.0)
        northing += 10000000.0;  // false northing for southern hemisphere

    // Format: 2 digits zone + band letter + 5 digits easting + 5 digits northing
    int eastingInt = qBound(0, static_cast<int>(qRound(easting)), 999999);
    int northingInt = qBound(0, static_cast<int>(qRound(northing)), 9999999);

    int eastingMajor = eastingInt / 1000;   // 3 digits (km)
    int eastingMinor = (eastingInt % 1000) / 10;  // 2 digits
    int northingMajor = northingInt / 1000;  // 3 digits (km)
    int northingMinor = (northingInt % 1000) / 10; // 2 digits

    return QStringLiteral("%1%2%3%4 %5%6")
        .arg(zone, 2, 10, QLatin1Char('0'))
        .arg(bandChar)
        .arg(eastingMajor, 3, 10, QLatin1Char('0'))
        .arg(eastingMinor, 2, 10, QLatin1Char('0'))
        .arg(northingMajor, 3, 10, QLatin1Char('0'))
        .arg(northingMinor, 2, 10, QLatin1Char('0'));
}

QString EntityInfoWidget::formatRemarks(const QString& raw) const
{
    QString text = raw;
    // Convert URLs to links
    static const QRegularExpression urlRe(QStringLiteral("https?://[^\\s]+"));
    text.replace(urlRe, QStringLiteral("<a href=\"\\0\" style=\"color:#4af;\">\\0</a>"));

    // Convert hashtags to clickable
    static const QRegularExpression hashRe(QStringLiteral("#(\\w+)"));
    text.replace(hashRe, QStringLiteral("<span class=\"hashtag\">#\\1</span>"));

    // Convert newlines to <br/>
    text.replace(QStringLiteral("\n"), QStringLiteral("<br/>"));

    return text;
}

QString EntityInfoWidget::formatBatteryBar(int percent) const
{
    int clamped = qBound(0, percent, 100);
    QString color;
    if (clamped < 20)
        color = QStringLiteral("#f44336");
    else if (clamped < 50)
        color = QStringLiteral("#ff9800");
    else
        color = QStringLiteral("#4caf50");

    return QStringLiteral(
        "<div class=\"section\" style=\"margin-top:10px;display:flex;align-items:center;gap:8px;\">"
        "<svg width=\"16\" height=\"16\" viewBox=\"0 0 24 24\" fill=\"white\">"
        "<path d=\"M16,20H8V6H16V20M16.67,4H15V2H9V4H7.33A0.67,0.67 0 0,0 6.67,4.67"
        "V20.33A0.67,0.67 0 0,0 7.33,21H16.67A0.67,0.67 0 0,0 17.33,20.33"
        "V4.67A0.67,0.67 0 0,0 16.67,4Z\"/></svg>"
        "<div style=\"flex-grow:1;height:14px;background:#333;border-radius:7px;"
        "border:1px solid #555;position:relative;overflow:hidden;\">"
        "<div style=\"width:%1%%;height:100%%;background:%2;"
        "transition:width 0.3s ease;\"></div>"
        "<div style=\"position:absolute;top:0;left:0;width:100%%;height:100%%;"
        "display:flex;align-items:center;justify-content:center;"
        "font-size:9px;font-weight:bold;color:white;"
        "text-shadow:1px 1px 2px black;\">%1%%</div></div></div>")
        .arg(clamped)
        .arg(color);
}

QString EntityInfoWidget::formatSpeedKmph(double speedMs) const
{
    double kmh = speedMs * 3.6;
    return QStringLiteral("%1 km/h").arg(kmh, 0, 'f', 1);
}

QString EntityInfoWidget::linkLabelForUid(const QString& uid) const
{
    QString lower = uid.toLower();
    if (lower.contains(QStringLiteral("gdacs")))
        return tr("View on GDACS");
    if (lower.contains(QStringLiteral("ais")))
        return tr("View vessel details");
    if (lower.contains(QStringLiteral("icao")))
        return tr("View aircraft details");
    return tr("Open link");
}

// ── Detail XML parsing helpers ──

QString EntityInfoWidget::parseDetail(const QString& detailXml,
                                      const QString& element) const
{
    if (detailXml.isEmpty()) return {};

    QXmlStreamReader xml(detailXml);
    while (!xml.atEnd() && !xml.hasError()) {
        xml.readNext();
        if (xml.isStartElement() && xml.name() == element) {
            return xml.readElementText(QXmlStreamReader::IncludeChildElements).trimmed();
        }
    }
    return {};
}

QString EntityInfoWidget::parseAttr(const QString& detailXml,
                                    const QString& element,
                                    const QString& attr) const
{
    if (detailXml.isEmpty()) return {};

    QXmlStreamReader xml(detailXml);
    while (!xml.atEnd() && !xml.hasError()) {
        xml.readNext();
        if (xml.isStartElement() && xml.name() == element) {
            return xml.attributes().value(attr).toString();
        }
    }
    return {};
}
