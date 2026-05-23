#include "gpsprovider.h"
#include <QDebug>
#include <QJsonArray>
#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QSerialPortInfo>

#ifdef Q_OS_WIN
static const QString DEFAULT_SERIAL_PORT = "COM1";
#else
static const QString DEFAULT_SERIAL_PORT = "/dev/ttyUSB0";
#endif

SerialGpsProvider::SerialGpsProvider(QObject* parent)
    : GpsProvider(parent)
    , m_portName(DEFAULT_SERIAL_PORT)
    , m_baudRate(9600)
    , m_serial(nullptr)
    , m_connected(false)
{
}

SerialGpsProvider::~SerialGpsProvider()
{
    disconnect();
}

void SerialGpsProvider::setConfig(const QJsonObject& config)
{
    m_portName = config.value("port").toString(DEFAULT_SERIAL_PORT);
    m_baudRate = config.value("baudRate").toInt(9600);
}

QJsonObject SerialGpsProvider::getConfig() const
{
    QJsonObject config;
    config["port"] = m_portName;
    config["baudRate"] = m_baudRate;
    return config;
}

bool SerialGpsProvider::connect()
{
    if (m_connected) return true;

    m_serial = new QSerialPort(m_portName, this);

    if (!m_serial->setBaudRate(m_baudRate)) {
        emit error("Failed to set baud rate " + QString::number(m_baudRate) + " on " + m_portName);
        delete m_serial;
        m_serial = nullptr;
        return false;
    }

    m_serial->setDataBits(QSerialPort::Data8);
    m_serial->setParity(QSerialPort::NoParity);
    m_serial->setStopBits(QSerialPort::OneStop);
    m_serial->setFlowControl(QSerialPort::NoFlowControl);

    if (!m_serial->open(QIODevice::ReadOnly)) {
        emit error("Failed to open " + m_portName + ": " + m_serial->errorString());
        delete m_serial;
        m_serial = nullptr;
        return false;
    }

    QObject::connect(m_serial, &QSerialPort::readyRead,
            this, &SerialGpsProvider::readSerialData);

    m_connected = true;
    emit connected();
    return true;
}

void SerialGpsProvider::disconnect()
{
    if (m_serial) {
        m_serial->close();
        delete m_serial;
        m_serial = nullptr;
    }

    m_connected = false;
    m_buffer.clear();
    emit disconnected();
}

bool SerialGpsProvider::isConnected() const
{
    return m_connected;
}

LocationData SerialGpsProvider::getCurrentLocation() const
{
    return m_currentLocation;
}

void SerialGpsProvider::readSerialData()
{
    if (!m_serial || !m_serial->isOpen()) return;

    QByteArray data = m_serial->readAll();
    m_buffer.append(data);

    int newlinePos;
    while ((newlinePos = m_buffer.indexOf('\n')) >= 0) {
        QString line = QString::fromUtf8(m_buffer.left(newlinePos)).trimmed();
        m_buffer = m_buffer.mid(newlinePos + 1);

        if (!line.isEmpty()) {
            parseNmeaLine(line);
        }
    }

    if (m_buffer.size() > 4096) {
        m_buffer = m_buffer.right(1024);
    }
}

void SerialGpsProvider::parseNmeaLine(const QString& line)
{
    if (!line.startsWith("$")) return;

    QString content = line.mid(1);
    int checksumPos = content.lastIndexOf('*');
    if (checksumPos > 0) {
        content = content.left(checksumPos);
    }

    QStringList parts = content.split(',');
    QString sentenceType = parts.value(0);

    if (sentenceType == "GPGGA" || sentenceType == "GNGGA") {
        parseGpgga(parts);
    } else if (sentenceType == "GPRMC" || sentenceType == "GNRMC") {
        parseGprmc(parts);
    } else if (sentenceType == "GPGSA" || sentenceType == "GNGSA") {
        parseGpgsa(parts);
    }
}

static double nmeaLatLonToDecimal(const QString& value, const QString& direction)
{
    if (value.isEmpty()) return 0.0;

    double val = value.toDouble();
    int degrees = static_cast<int>(val / 100);
    double minutes = val - (degrees * 100);
    double decimal = degrees + (minutes / 60.0);

    if (direction == "S" || direction == "W") {
        decimal = -decimal;
    }

    return decimal;
}

void SerialGpsProvider::parseGpgga(const QStringList& parts)
{
    if (parts.size() < 15) return;

    QString latStr = parts.value(2);
    QString latDir = parts.value(3);
    QString lonStr = parts.value(4);
    QString lonDir = parts.value(5);
    QString fixQuality = parts.value(6);
    QString sats = parts.value(7);
    QString hdop = parts.value(8);
    QString alt = parts.value(9);

    if (fixQuality == "0") {
        m_currentLocation.valid = false;
        return;
    }

    m_currentLocation.latitude = nmeaLatLonToDecimal(latStr, latDir);
    m_currentLocation.longitude = nmeaLatLonToDecimal(lonStr, lonDir);
    m_currentLocation.altitude = alt.toDouble();
    m_currentLocation.satellites = sats.toInt();
    m_currentLocation.hdop = hdop.toDouble();
    m_currentLocation.valid = true;
    m_currentLocation.source = providerId();

    emit locationUpdated(m_currentLocation);
}

void SerialGpsProvider::parseGprmc(const QStringList& parts)
{
    if (parts.size() < 12) return;

    QString status = parts.value(2);
    if (status != "A") {
        m_currentLocation.valid = false;
        return;
    }

    QString latStr = parts.value(3);
    QString latDir = parts.value(4);
    QString lonStr = parts.value(5);
    QString lonDir = parts.value(6);
    QString speedKnots = parts.value(7);
    QString course = parts.value(8);

    m_currentLocation.latitude = nmeaLatLonToDecimal(latStr, latDir);
    m_currentLocation.longitude = nmeaLatLonToDecimal(lonStr, lonDir);
    m_currentLocation.speed = speedKnots.toDouble() * 1.852;
    m_currentLocation.course = course.toDouble();
    m_currentLocation.valid = true;
    m_currentLocation.source = providerId();

    emit locationUpdated(m_currentLocation);
}

void SerialGpsProvider::parseGpgsa(const QStringList& parts)
{
    if (parts.size() < 18) return;

    int satsUsed = 0;
    for (int i = 3; i <= 14; i++) {
        if (!parts.value(i).isEmpty()) {
            satsUsed++;
        }
    }

    if (satsUsed > 0) {
        m_currentLocation.satellites = satsUsed;
    }
}

GpsdProvider::GpsdProvider(QObject* parent)
    : GpsProvider(parent)
    , m_host("localhost")
    , m_port(2947)
    , m_tcpSocket(nullptr)
    , m_connected(false)
{
}

GpsdProvider::~GpsdProvider()
{
    disconnect();
}

void GpsdProvider::setConfig(const QJsonObject& config)
{
    m_host = config.value("host").toString("localhost");
    m_port = config.value("port").toInt(2947);
}

QJsonObject GpsdProvider::getConfig() const
{
    QJsonObject config;
    config["host"] = m_host;
    config["port"] = m_port;
    return config;
}

bool GpsdProvider::connect()
{
    if (m_connected) return true;

    QTcpSocket* socket = new QTcpSocket(this);
    socket->connectToHost(m_host, m_port);

    if (!socket->waitForConnected(3000)) {
        emit error("Failed to connect to GPSD at " + m_host + ":" + QString::number(m_port));
        delete socket;
        return false;
    }

    m_tcpSocket = socket;
    m_connected = true;

    socket->write("?WATCH={\"enable\":true,\"json\":true}\r\n");
    socket->write("?POLL;\r\n");

    QObject::connect(socket, &QTcpSocket::readyRead, this, &GpsdProvider::readFromSocket);

    QTimer* pollTimer = new QTimer(this);
    QObject::connect(pollTimer, &QTimer::timeout, this, [this]() {
        if (m_tcpSocket) {
            static_cast<QTcpSocket*>(m_tcpSocket)->write("?POLL;\r\n");
        }
    });
    pollTimer->start(1000);

    emit connected();
    return true;
}

void GpsdProvider::disconnect()
{
    if (m_tcpSocket) {
        QTcpSocket* socket = static_cast<QTcpSocket*>(m_tcpSocket);
        socket->write("?WATCH={\"enable\":false}\r\n");
        socket->close();
        socket->deleteLater();
        m_tcpSocket = nullptr;
    }
    m_connected = false;
    emit disconnected();
}

bool GpsdProvider::isConnected() const
{
    return m_connected;
}

LocationData GpsdProvider::getCurrentLocation() const
{
    return m_currentLocation;
}

void GpsdProvider::readFromSocket()
{
    QTcpSocket* socket = static_cast<QTcpSocket*>(m_tcpSocket);
    if (!socket) return;

    while (socket->canReadLine()) {
        QString line = QString::fromUtf8(socket->readLine()).trimmed();
        if (line.isEmpty()) continue;

        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(line.toUtf8(), &error);
        if (error.error != QJsonParseError::NoError) continue;

        QJsonObject obj = doc.object();
        QString type = obj.value("class").toString();

        if (type == "TPV") {
            if (obj.contains("lat") && obj.contains("lon")) {
                m_currentLocation.latitude = obj.value("lat").toDouble();
                m_currentLocation.longitude = obj.value("lon").toDouble();
                m_currentLocation.altitude = obj.value("alt").toDouble(0.0);
                m_currentLocation.speed = obj.value("speed").toDouble(0.0);
                m_currentLocation.course = obj.value("track").toDouble(0.0);
                QString mode = obj.value("mode").toString();
                m_currentLocation.valid = (mode == "2" || mode == "3");
                m_currentLocation.source = providerId();

                emit locationUpdated(m_currentLocation);
            }
        } else if (type == "SKY") {
            QJsonArray satellites = obj.value("satellites").toArray();
            m_currentLocation.satellites = satellites.size();
            if (obj.contains("xdop")) {
                m_currentLocation.hdop = obj.value("xdop").toDouble(99.9);
            }
        }
    }
}

ManualProvider::ManualProvider(QObject* parent)
    : GpsProvider(parent)
    , m_connected(false)
{
    m_currentLocation.source = "manual";
}

bool ManualProvider::connect()
{
    m_connected = true;
    if (m_currentLocation.valid) {
        emit locationUpdated(m_currentLocation);
    }
    emit connected();
    return true;
}

void ManualProvider::disconnect()
{
    m_connected = false;
    emit disconnected();
}

bool ManualProvider::isConnected() const
{
    return m_connected;
}

LocationData ManualProvider::getCurrentLocation() const
{
    return m_currentLocation;
}

void ManualProvider::setConfig(const QJsonObject& config)
{
    m_currentLocation.latitude = config.value("latitude").toDouble(0.0);
    m_currentLocation.longitude = config.value("longitude").toDouble(0.0);
    m_currentLocation.altitude = config.value("altitude").toDouble(0.0);
    m_currentLocation.valid = config.value("valid").toBool(false);
}

QJsonObject ManualProvider::getConfig() const
{
    QJsonObject config;
    config["latitude"] = m_currentLocation.latitude;
    config["longitude"] = m_currentLocation.longitude;
    config["altitude"] = m_currentLocation.altitude;
    config["valid"] = m_currentLocation.valid;
    return config;
}

void ManualProvider::setLocation(double lat, double lon, double alt)
{
    m_currentLocation.latitude = lat;
    m_currentLocation.longitude = lon;
    m_currentLocation.altitude = alt;
    m_currentLocation.valid = true;
    m_currentLocation.source = "manual";

    emit locationUpdated(m_currentLocation);
}

#ifdef QT_POSITIONING_LIB

SystemPositionProvider::SystemPositionProvider(QObject* parent)
    : GpsProvider(parent)
    , m_source(nullptr)
    , m_connected(false)
{
    m_source = QGeoPositionInfoSource::createDefaultSource(this);
    if (m_source) {
        m_sourceName = m_source->metaObject()->className();
        QObject::connect(m_source, &QGeoPositionInfoSource::positionUpdated,
                this, &SystemPositionProvider::onPositionUpdated);
        QObject::connect(m_source,
                SIGNAL(error(QGeoPositionInfoSource::Error)),
                this,
                SLOT(onSourceError(QGeoPositionInfoSource::Error)));
    }
}

SystemPositionProvider::~SystemPositionProvider()
{
    disconnect();
}

QString SystemPositionProvider::providerId() const
{
    return "system_" + m_sourceName;
}

bool SystemPositionProvider::connect()
{
    if (m_connected) return true;

    if (!m_source) {
        m_source = QGeoPositionInfoSource::createDefaultSource(this);
        if (!m_source) {
            emit error("No system location source available");
            return false;
        }
        QObject::connect(m_source, &QGeoPositionInfoSource::positionUpdated,
                this, &SystemPositionProvider::onPositionUpdated);
        QObject::connect(m_source,
                SIGNAL(error(QGeoPositionInfoSource::Error)),
                this,
                SLOT(onSourceError(QGeoPositionInfoSource::Error)));
        m_sourceName = m_source->metaObject()->className();
    }

    m_source->startUpdates();
    m_connected = true;
    emit connected();
    return true;
}

void SystemPositionProvider::disconnect()
{
    if (m_source)
        m_source->stopUpdates();
    m_connected = false;
    m_currentLocation = LocationData();
    emit disconnected();
}

bool SystemPositionProvider::isConnected() const
{
    return m_connected;
}

LocationData SystemPositionProvider::getCurrentLocation() const
{
    return m_currentLocation;
}

void SystemPositionProvider::setConfig(const QJsonObject& config)
{
    Q_UNUSED(config);
}

QJsonObject SystemPositionProvider::getConfig() const
{
    return QJsonObject();
}

void SystemPositionProvider::onPositionUpdated(const QGeoPositionInfo& info)
{
    if (!info.isValid()) return;

    m_currentLocation.latitude = info.coordinate().latitude();
    m_currentLocation.longitude = info.coordinate().longitude();
    m_currentLocation.altitude = info.coordinate().altitude();
    m_currentLocation.speed = info.attribute(QGeoPositionInfo::GroundSpeed);
    m_currentLocation.course = info.attribute(QGeoPositionInfo::Direction);
    m_currentLocation.hdop = info.attribute(QGeoPositionInfo::HorizontalAccuracy);
    m_currentLocation.valid = true;
    m_currentLocation.source = providerId();

    emit locationUpdated(m_currentLocation);
}

void SystemPositionProvider::onSourceError(QGeoPositionInfoSource::Error err)
{
    if (err == QGeoPositionInfoSource::AccessError) {
        emit error("Location access denied. Check system privacy settings.");
    } else if (err != QGeoPositionInfoSource::NoError) {
        emit error("System location error: " + QString::number(static_cast<int>(err)));
    }
}

#endif
