#include "natsclient.h"
#include <QDebug>
#include <QByteArray>

NatsClient::NatsClient(QObject* parent)
    : QObject(parent)
    , m_connection(nullptr)
    , m_connected(false)
{
    nats_Open(0);
}

NatsClient::~NatsClient()
{
    disconnect();
    if (m_connection) {
        natsConnection_Destroy(m_connection);
    }
    nats_Close();
}

bool NatsClient::connectToServer(const QString& url)
{
    QMutexLocker lock(&m_mutex);

    if (m_connection) {
        natsConnection_Destroy(m_connection);
        m_connection = nullptr;
    }

    QString fullUrl;
    if (url.contains("://")) {
        fullUrl = url;
    } else {
        fullUrl = QString("nats://%1:%2").arg(url).arg(4222);
    }

    QByteArray urlBytes = fullUrl.toUtf8();
    natsStatus status = natsConnection_ConnectTo(&m_connection, urlBytes.constData());

    if (status != NATS_OK) {
        m_lastError = QString::fromUtf8(natsStatus_GetText(status));
        emit error(m_lastError);
        m_connected = false;
        return false;
    }

    m_connected = true;
    emit connected();
    return true;
}

void NatsClient::disconnect()
{
    QMutexLocker lock(&m_mutex);

    for (auto it = m_subscriptions.begin(); it != m_subscriptions.end(); ++it) {
        natsSubscription_Destroy(it.value());
    }
    m_subscriptions.clear();

    if (m_connection) {
        natsConnection_Close(m_connection);
        m_connection = nullptr;
        m_connected = false;
        emit disconnected();
    }
}

bool NatsClient::subscribe(const QString& topic)
{
    QMutexLocker lock(&m_mutex);

    if (!m_connected || !m_connection) {
        emit error("Not connected");
        return false;
    }

    if (m_subscriptions.contains(topic)) {
        return true;
    }

    natsSubscription* sub = nullptr;
    QByteArray topicBytes = topic.toUtf8();

    natsStatus status = natsConnection_Subscribe(&sub, m_connection, topicBytes.constData(), messageHandler, this);

    if (status != NATS_OK) {
        m_lastError = QString::fromUtf8(natsStatus_GetText(status));
        emit error(m_lastError);
        return false;
    }

    m_subscriptions[topic] = sub;
    qDebug() << "NatsClient: Subscribed to" << topic;
    return true;
}

bool NatsClient::unsubscribe(const QString& topic)
{
    QMutexLocker lock(&m_mutex);

    if (m_subscriptions.contains(topic)) {
        natsSubscription_Destroy(m_subscriptions[topic]);
        m_subscriptions.remove(topic);
        qDebug() << "NatsClient: Unsubscribed from" << topic;
    }
    return true;
}

bool NatsClient::publish(const QString& topic, const QString& payload)
{
    QMutexLocker lock(&m_mutex);

    if (!m_connected || !m_connection) {
        emit error("Not connected");
        return false;
    }

    QByteArray topicBytes = topic.toUtf8();
    QByteArray payloadBytes = payload.toUtf8();

    natsStatus status = natsConnection_Publish(m_connection, topicBytes.constData(),
                                                 payloadBytes.constData(), payloadBytes.length());

    if (status != NATS_OK) {
        m_lastError = QString::fromUtf8(natsStatus_GetText(status));
        emit error(m_lastError);
        return false;
    }

    natsConnection_Flush(m_connection);
    return true;
}

void NatsClient::messageHandler(natsConnection* nc, natsSubscription* sub, natsMsg* msg, void* closure)
{
    if (closure) {
        NatsClient* client = static_cast<NatsClient*>(closure);
        client->handleMessage(msg);
    }
}

void NatsClient::handleMessage(natsMsg* msg)
{
    if (!msg) return;

    QString subject = QString::fromUtf8(natsMsg_GetSubject(msg));
    const char* data = natsMsg_GetData(msg);
    int len = natsMsg_GetDataLength(msg);

    QString payload = QString::fromUtf8(data, len);

    natsMsg_Destroy(msg);

    emit messageReceived(subject, payload);
}