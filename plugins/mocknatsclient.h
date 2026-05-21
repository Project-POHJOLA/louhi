#ifndef MOCKNATSCLIENT_H
#define MOCKNATSCLIENT_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>

class MockNatsClient : public QObject
{
    Q_OBJECT

public:
    explicit MockNatsClient(QObject* parent = nullptr);
    ~MockNatsClient();

    bool connectToServer(const QString& url, int port = 4222);
    void disconnect();

    bool isConnected() const { return m_connected; }

    bool subscribe(const QString& topic);
    bool unsubscribe(const QString& topic);
    bool publish(const QString& topic, const QString& payload);

    void setServerUrl(const QString& url) { m_serverUrl = url; }
    void setPort(int port) { m_port = port; }

signals:
    void connected();
    void disconnected();
    void messageReceived(const QString& topic, const QString& payload);
    void error(const QString& error);

public slots:
    void simulateIncomingMessage(const QString& topic, const QString& payload);

private slots:
    void onTimeout();

private:
    QString m_serverUrl;
    int m_port;
    bool m_connected;
    QStringList m_subscriptions;
    QTimer* m_testTimer;
};

#endif