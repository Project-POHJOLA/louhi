#include "mocknatsclient.h"
#include <QDebug>

MockNatsClient::MockNatsClient(QObject* parent)
    : QObject(parent)
    , m_port(4222)
    , m_connected(false)
    , m_testTimer(new QTimer(this))
{
    QObject::connect(m_testTimer, &QTimer::timeout, this, &MockNatsClient::onTimeout);
}

MockNatsClient::~MockNatsClient()
{
    disconnect();
}

bool MockNatsClient::connectToServer(const QString& url, int port)
{
    m_serverUrl = url;
    m_port = port;

    qDebug() << "MockNatsClient: Connecting to" << url << ":" << port;

    m_connected = true;
    emit connected();

    return true;
}

void MockNatsClient::disconnect()
{
    m_connected = false;
    m_subscriptions.clear();
    m_testTimer->stop();
    emit disconnected();
}

bool MockNatsClient::subscribe(const QString& topic)
{
    if (!m_connected) {
        emit error("Not connected");
        return false;
    }

    if (!m_subscriptions.contains(topic)) {
        m_subscriptions.append(topic);
        qDebug() << "MockNatsClient: Subscribed to" << topic;
    }
    return true;
}

bool MockNatsClient::unsubscribe(const QString& topic)
{
    m_subscriptions.removeAll(topic);
    qDebug() << "MockNatsClient: Unsubscribed from" << topic;
    return true;
}

bool MockNatsClient::publish(const QString& topic, const QString& payload)
{
    if (!m_connected) {
        emit error("Not connected");
        return false;
    }

    qDebug() << "MockNatsClient: Published to" << topic << ":" << payload;
    emit messageReceived(topic, payload);
    return true;
}

void MockNatsClient::simulateIncomingMessage(const QString& topic, const QString& payload)
{
    if (m_connected && m_subscriptions.contains(topic)) {
        emit messageReceived(topic, payload);
    }
}

void MockNatsClient::onTimeout()
{
    static int counter = 0;
    QString topic = "test.message";
    QString payload = QString("Test message #%1 from mock NATS").arg(++counter);
    emit messageReceived(topic, payload);
}