#include "cotmessage.h"
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QBuffer>
#include <QDebug>

QString CoTMessageBuilder::buildPositionReport(
    const QString& uid,
    const QString& callsign,
    const QString& type,
    const QString& how,
    double lat,
    double lon,
    double hae,
    double ce,
    double le,
    const QString& groupName,
    const QString& role,
    const QString& takvDevice,
    const QString& takvOs,
    const QString& takvPlatform,
    const QString& takvVersion,
    const QString& remarks
) {
    QDateTime now = QDateTime::currentDateTimeUtc();
    QString stale = now.addSecs(120).toString(Qt::ISODate);
    QString timeStr = now.toString(Qt::ISODate);

    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly);
    QXmlStreamWriter writer(&buffer);
    writer.setAutoFormatting(true);
    writer.writeStartDocument();
    writer.writeStartElement("event");
    writer.writeAttribute("version", "2.0");
    writer.writeAttribute("uid", uid);
    writer.writeAttribute("type", type);
    writer.writeAttribute("how", how);
    writer.writeAttribute("time", timeStr);
    writer.writeAttribute("start", timeStr);
    writer.writeAttribute("stale", stale);

    writer.writeStartElement("point");
    writer.writeAttribute("lat", QString::number(lat, 'f', 8));
    writer.writeAttribute("lon", QString::number(lon, 'f', 8));
    writer.writeAttribute("hae", QString::number(hae, 'f', 1));
    writer.writeAttribute("ce", QString::number(ce, 'f', 1));
    writer.writeAttribute("le", QString::number(le, 'f', 1));
    writer.writeEndElement();

    writer.writeStartElement("detail");

    writer.writeStartElement("contact");
    writer.writeAttribute("callsign", callsign);
    writer.writeAttribute("endpoint", "*");
    writer.writeEndElement();

    if (!groupName.isEmpty() || !role.isEmpty()) {
        writer.writeStartElement("__group");
        writer.writeAttribute("name", groupName);
        writer.writeAttribute("role", role);
        writer.writeEndElement();
    }

    if (!takvDevice.isEmpty() || !takvPlatform.isEmpty()) {
        writer.writeStartElement("takv");
        writer.writeAttribute("device", takvDevice);
        writer.writeAttribute("os", takvOs);
        writer.writeAttribute("platform", takvPlatform);
        writer.writeAttribute("version", takvVersion);
        writer.writeEndElement();
    }

    if (!remarks.isEmpty()) {
        writer.writeStartElement("remarks");
        writer.writeAttribute("source", callsign);
        writer.writeAttribute("to", "*");
        writer.writeAttribute("time", timeStr);
        writer.writeCharacters(remarks);
        writer.writeEndElement();
    }

    writer.writeEndElement();
    writer.writeEndElement();
    writer.writeEndDocument();

    return QString::fromUtf8(buffer.data());
}

QString CoTMessageBuilder::buildChatMessage(
    const QString& uid,
    const QString& callsign,
    const QString& chatGroup,
    const QString& message,
    const QString& toUid
) {
    QDateTime now = QDateTime::currentDateTimeUtc();
    QString stale = now.addSecs(120).toString(Qt::ISODate);
    QString timeStr = now.toString(Qt::ISODate);

    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly);
    QXmlStreamWriter writer(&buffer);
    writer.setAutoFormatting(true);
    writer.writeStartDocument();
    writer.writeStartElement("event");
    writer.writeAttribute("version", "2.0");
    writer.writeAttribute("uid", uid);
    writer.writeAttribute("type", "b-m-p-s-p");
    writer.writeAttribute("how", "h-g-i-g-o");
    writer.writeAttribute("time", timeStr);
    writer.writeAttribute("start", timeStr);
    writer.writeAttribute("stale", stale);

    writer.writeStartElement("point");
    writer.writeAttribute("lat", "0.0");
    writer.writeAttribute("lon", "0.0");
    writer.writeAttribute("hae", "9999999.0");
    writer.writeAttribute("ce", "9999999.0");
    writer.writeAttribute("le", "9999999.0");
    writer.writeEndElement();

    writer.writeStartElement("detail");

    writer.writeStartElement("__chat");
    writer.writeAttribute("chatroom", chatGroup);
    writer.writeAttribute("group", chatGroup);
    writer.writeAttribute("id", uid);
    if (!toUid.isEmpty()) {
        writer.writeAttribute("to", toUid);
    }
    writer.writeStartElement("chatgrp");
    writer.writeAttribute("id", uid);
    writer.writeEndElement();
    writer.writeEndElement();

    writer.writeStartElement("link");
    writer.writeAttribute("uid", uid);
    writer.writeAttribute("relation", "p-p");
    writer.writeAttribute("type", "b-m-p-s-p");
    writer.writeEndElement();

    writer.writeStartElement("remarks");
    writer.writeAttribute("source", callsign);
    writer.writeAttribute("to", "*");
    writer.writeAttribute("time", timeStr);
    writer.writeCharacters(message);
    writer.writeEndElement();

    writer.writeEndElement();
    writer.writeEndElement();
    writer.writeEndDocument();

    return QString::fromUtf8(buffer.data());
}

CoTMessage CoTMessageParser::parse(const QString& xml) {
    CoTMessage msg;
    msg.rawXml = xml;

    QXmlStreamReader reader(xml);

    while (!reader.atEnd() && !reader.hasError()) {
        if (reader.readNext() != QXmlStreamReader::StartElement)
            continue;

        if (reader.name() == QLatin1String("event")) {
            QXmlStreamAttributes attrs = reader.attributes();
            msg.uid = attrs.value("uid").toString();
            msg.how = attrs.value("how").toString();

            QString typeStr = attrs.value("type").toString();
            if (typeStr == "a-f-G-E-V-C") msg.eventType = CotEventType::a_f_G_E_V_C;
            else if (typeStr == "a-u-G-F-I") msg.eventType = CotEventType::a_u_G_F_I;
            else if (typeStr == "b-m-p-s-p") msg.eventType = CotEventType::b_m_p_s_p;
            else if (typeStr == "t-x-c-o-n") msg.eventType = CotEventType::t_x_c_o_n;
            else msg.eventType = CotEventType::unknown;

            msg.time = QDateTime::fromString(attrs.value("time").toString(), Qt::ISODate);
            msg.start = QDateTime::fromString(attrs.value("start").toString(), Qt::ISODate);
            msg.stale = QDateTime::fromString(attrs.value("stale").toString(), Qt::ISODate);
        }
        else if (reader.name() == QLatin1String("point")) {
            QXmlStreamAttributes attrs = reader.attributes();
            msg.point.lat = attrs.value("lat").toDouble();
            msg.point.lon = attrs.value("lon").toDouble();
            msg.point.hae = attrs.value("hae").toDouble();
            msg.point.ce = attrs.value("ce").toDouble();
            msg.point.le = attrs.value("le").toDouble();
        }
        else if (reader.name() == QLatin1String("contact")) {
            QXmlStreamAttributes attrs = reader.attributes();
            msg.contact.callsign = attrs.value("callsign").toString();
            msg.contact.endpoint = attrs.value("endpoint").toString();
        }
        else if (reader.name() == QLatin1String("remarks")) {
            msg.remarks = reader.readElementText();
        }
    }

    if (reader.hasError()) {
        qDebug() << "CoT: XML parse error:" << reader.errorString();
    }

    return msg;
}

bool CoTMessageParser::isValid(const QString& xml) {
    if (xml.isEmpty()) {
        return false;
    }

    QString trimmed = xml.trimmed();
    if (trimmed.isEmpty()) {
        return false;
    }

    QXmlStreamReader reader(trimmed);

    while (!reader.atEnd() && !reader.hasError()) {
        if (reader.readNext() != QXmlStreamReader::StartElement)
            continue;

        if (reader.name() == QLatin1String("event")) {
            if (reader.attributes().value("uid").isEmpty()) {
                qDebug() << "CoT: Missing uid attribute:" << trimmed.left(200);
                return false;
            }
            return true;
        }
    }

    if (reader.hasError()) {
        qDebug() << "CoT: XML parse error:" << reader.errorString();
        qDebug() << "CoT: Invalid XML:" << trimmed.left(200);
        return false;
    }

    qDebug() << "CoT: Root element is not event (no event element found):" << trimmed.left(200);
    return false;
}
