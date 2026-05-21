#ifndef GPSPROVIDER_H
#define GPSPROVIDER_H

#include <QObject>
#include <QString>
#include <QJsonObject>

struct LocationData {
    double latitude;
    double longitude;
    double altitude;
    double speed;
    double course;
    double hdop;
    int satellites;
    bool valid;
    QString source;

    LocationData()
        : latitude(0.0), longitude(0.0), altitude(0.0)
        , speed(0.0), course(0.0), hdop(99.9)
        , satellites(0), valid(false) {}
};

class GpsProvider : public QObject
{
    Q_OBJECT

public:
    explicit GpsProvider(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~GpsProvider() = default;

    virtual QString providerType() const = 0;
    virtual QString providerId() const = 0;
    virtual bool connect() = 0;
    virtual void disconnect() = 0;
    virtual bool isConnected() const = 0;
    virtual LocationData getCurrentLocation() const = 0;

    virtual void setConfig(const QJsonObject& config) = 0;
    virtual QJsonObject getConfig() const = 0;

signals:
    void locationUpdated(const LocationData& location);
    void connected();
    void disconnected();
    void error(const QString& message);
};

class SerialGpsProvider : public GpsProvider
{
    Q_OBJECT

public:
    explicit SerialGpsProvider(QObject* parent = nullptr);
    ~SerialGpsProvider();

    QString providerType() const override { return "serial"; }
    QString providerId() const override { return m_portName; }
    bool connect() override;
    void disconnect() override;
    bool isConnected() const override;
    LocationData getCurrentLocation() const override;

    void setConfig(const QJsonObject& config) override;
    QJsonObject getConfig() const override;

private slots:
    void readSerialData();

private:
    void parseNmeaLine(const QString& line);
    void parseGpgga(const QStringList& parts);
    void parseGprmc(const QStringList& parts);
    void parseGpgsa(const QStringList& parts);
    bool configurePort(int fd);

    QString m_portName;
    int m_baudRate;
    int m_fd;
    void* m_notifier;
    bool m_connected;
    LocationData m_currentLocation;
    QByteArray m_buffer;
};

class GpsdProvider : public GpsProvider
{
    Q_OBJECT

public:
    explicit GpsdProvider(QObject* parent = nullptr);
    ~GpsdProvider();

    QString providerType() const override { return "gpsd"; }
    QString providerId() const override { return QString("gpsd://%1:%2").arg(m_host).arg(m_port); }
    bool connect() override;
    void disconnect() override;
    bool isConnected() const override;
    LocationData getCurrentLocation() const override;

    void setConfig(const QJsonObject& config) override;
    QJsonObject getConfig() const override;

private slots:
    void readFromSocket();

private:
    QString m_host;
    int m_port;
    void* m_tcpSocket;
    bool m_connected;
    LocationData m_currentLocation;
};

class ManualProvider : public GpsProvider
{
    Q_OBJECT

public:
    explicit ManualProvider(QObject* parent = nullptr);
    ~ManualProvider() = default;

    QString providerType() const override { return "manual"; }
    QString providerId() const override { return "manual"; }
    bool connect() override;
    void disconnect() override;
    bool isConnected() const override;
    LocationData getCurrentLocation() const override;

    void setConfig(const QJsonObject& config) override;
    QJsonObject getConfig() const override;

    void setLocation(double lat, double lon, double alt = 0.0);

private:
    bool m_connected;
    LocationData m_currentLocation;
};

#endif
