#include "cotmessage.h"
#include <QDomElement>
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

QString CoTMessageBuilder::buildPing(
    const QString& uid,
    const QString& callsign
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
    writer.writeAttribute("type", "t-x-c-o-n");
    writer.writeAttribute("how", "m-g");
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
    writer.writeStartElement("contact");
    writer.writeAttribute("callsign", callsign);
    writer.writeEndElement();
    writer.writeEndElement();

    writer.writeEndElement();
    writer.writeEndDocument();

    return QString::fromUtf8(buffer.data());
}

CoTMessage CoTMessageParser::parse(const QString& xml) {
    CoTMessage msg;
    msg.rawXml = xml;

    QDomDocument doc;
    if (!doc.setContent(xml)) {
        return msg;
    }

    QDomElement root = doc.documentElement();
    if (root.tagName() != "event") {
        return msg;
    }

    msg.uid = root.attribute("uid");
    msg.how = root.attribute("how");

    QString typeStr = root.attribute("type");
    if (typeStr == "a-f-G-E-V-C") msg.eventType = CotEventType::a_f_G_E_V_C;
    else if (typeStr == "a-u-G-F-I") msg.eventType = CotEventType::a_u_G_F_I;
    else if (typeStr == "b-m-p-s-p") msg.eventType = CotEventType::b_m_p_s_p;
    else if (typeStr == "t-x-c-o-n") msg.eventType = CotEventType::t_x_c_o_n;
    else msg.eventType = CotEventType::unknown;

    msg.time = QDateTime::fromString(root.attribute("time"), Qt::ISODate);
    msg.start = QDateTime::fromString(root.attribute("start"), Qt::ISODate);
    msg.stale = QDateTime::fromString(root.attribute("stale"), Qt::ISODate);

    QDomElement detail = root.firstChildElement("detail");
    if (!detail.isNull()) {
        QDomElement contact = detail.firstChildElement("contact");
        if (!contact.isNull()) {
            msg.contact.callsign = contact.attribute("callsign");
            msg.contact.endpoint = contact.attribute("endpoint");
        }

        QDomElement remarks = detail.firstChildElement("remarks");
        if (!remarks.isNull()) {
            msg.remarks = remarks.text();
        }
    }

    QDomElement point = root.firstChildElement("point");
    if (!point.isNull()) {
        msg.point.lat = point.attribute("lat").toDouble();
        msg.point.lon = point.attribute("lon").toDouble();
        msg.point.hae = point.attribute("hae").toDouble();
        msg.point.ce = point.attribute("ce").toDouble();
        msg.point.le = point.attribute("le").toDouble();
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

    QDomDocument doc;
    QString errorMsg;
    int errorLine = 0, errorCol = 0;
    if (!doc.setContent(trimmed, &errorMsg, &errorLine, &errorCol)) {
        qDebug() << "CoT: XML parse error at line" << errorLine << "col" << errorCol << ":" << errorMsg;
        qDebug() << "CoT: Invalid XML:" << trimmed.left(200);
        return false;
    }

    QDomElement root = doc.documentElement();
    if (root.tagName() != "event") {
        qDebug() << "CoT: Root element is" << root.tagName() << "(expected event):" << trimmed.left(200);
        return false;
    }

    if (root.attribute("uid").isEmpty()) {
        qDebug() << "CoT: Missing uid attribute:" << trimmed.left(200);
        return false;
    }

    return true;
}
