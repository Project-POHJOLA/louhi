#ifndef TAKSERVERCONNECTION_H
#define TAKSERVERCONNECTION_H

#include <QObject>
#include <QString>
#include <QTcpSocket>
#include <QSslSocket>
#include <QSslCertificate>
#include <QSslKey>
#include <QSslConfiguration>
#include <QMutex>
#include <QTimer>
#include <QByteArray>

struct TakServerConfig {
    QString id;
    QString name;
    QString address;
    int port;
    QString certFilePath;
    QString certPassword;
    QByteArray certData;
    QString callsign;
    QString color;
    QString role;
    QString cotType;
    bool autoConnect;
    bool debugLogging;
};

class TakServerConnection : public QObject
{
    Q_OBJECT

public:
    explicit TakServerConnection(const TakServerConfig& config, QObject* parent = nullptr);
    ~TakServerConnection();

    void connectToServer();
    void disconnect();
    void sendCoT(const QString& xml);

    bool isConnected() const { return m_connected; }
    TakServerConfig config() const { return m_config; }
    void updateConfig(const TakServerConfig& config);

    QString statusText() const;

signals:
    void connected();
    void disconnected();
    void messageReceived(const QString& xml);
    void connectionError(const QString& error);
    void statusChanged(const QString& status);

private slots:
    void onConnected();
    void onDisconnected();
    void onError(QAbstractSocket::SocketError error);
    void onSslErrors(const QList<QSslError>& errors);
    void onReadyRead();
    void onReconnectTimeout();

private:
    void setupSsl();
    void parseBuffer();
    QString generateUid() const;
    void scheduleReconnect();

    TakServerConfig m_config;
    QSslSocket* m_socket;
    bool m_connected;
    bool m_userDisconnected;
    QString m_statusText;
    QMutex m_mutex;

    QByteArray m_readBuffer;

    QTimer* m_reconnectTimer;
    int m_reconnectAttempts;
    int m_reconnectDelayMs;
    static constexpr int kReconnectMinDelayMs = 1000;
    static constexpr int kReconnectMaxDelayMs = 60000;

    QString m_uid;
};

#endif
