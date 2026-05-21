#ifndef NATSCLIENT_H
#define NATSCLIENT_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QMap>
#include <QMutex>

#include <nats/nats.h>

class NatsClient : public QObject
{
    Q_OBJECT

public:
    explicit NatsClient(QObject* parent = nullptr);
    ~NatsClient();

    bool connectToServer(const QString& url);
    void disconnect();

    bool isConnected() const { return m_connected; }

    bool subscribe(const QString& topic);
    bool unsubscribe(const QString& topic);
    bool publish(const QString& topic, const QString& payload);

    QString lastError() const { return m_lastError; }

signals:
    void connected();
    void disconnected();
    void messageReceived(const QString& topic, const QString& payload);
    void error(const QString& error);

private:
    natsConnection* m_connection;
    QMap<QString, natsSubscription*> m_subscriptions;
    bool m_connected;
    QString m_lastError;
    QMutex m_mutex;

    static void messageHandler(natsConnection* nc, natsSubscription* sub, natsMsg* msg, void* closure);
    void handleMessage(natsMsg* msg);
};

#endif