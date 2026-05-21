#include "takserverconnection.h"
#include "cotmessage.h"
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QSslKey>
#include <QSslCertificate>
#include <QUuid>
#include <QBuffer>
#include <openssl/provider.h>

TakServerConnection::TakServerConnection(const TakServerConfig& config, QObject* parent)
    : QObject(parent)
    , m_config(config)
    , m_socket(nullptr)
    , m_connected(false)
    , m_userDisconnected(false)
    , m_statusText("Idle")
    , m_reconnectTimer(new QTimer(this))
    , m_reconnectAttempts(0)
    , m_reconnectDelayMs(kReconnectMinDelayMs)
{
    m_uid = QString("tak-%1-%2").arg(m_config.id).arg(QUuid::createUuid().toString(QUuid::WithoutBraces).left(8));

    m_reconnectTimer->setSingleShot(true);
    connect(m_reconnectTimer, &QTimer::timeout, this, &TakServerConnection::onReconnectTimeout);
}

TakServerConnection::~TakServerConnection()
{
    disconnect();
}

void TakServerConnection::updateConfig(const TakServerConfig& config)
{
    bool wasConnected = m_connected;
    if (wasConnected) {
        disconnect();
    }

    m_config = config;
    m_uid = QString("tak-%1-%2").arg(m_config.id).arg(QUuid::createUuid().toString(QUuid::WithoutBraces).left(8));

    if (wasConnected && m_config.autoConnect) {
        connectToServer();
    }
}

void TakServerConnection::scheduleReconnect()
{
    if (m_userDisconnected || !m_config.autoConnect) {
        return;
    }

    if (m_reconnectDelayMs > kReconnectMinDelayMs) {
        m_statusText = QString("Reconnecting in %1s...").arg(m_reconnectDelayMs / 1000);
    } else {
        m_statusText = "Reconnecting...";
    }
    emit statusChanged(m_statusText);

    m_reconnectTimer->start(m_reconnectDelayMs);
    m_reconnectDelayMs = qMin(m_reconnectDelayMs * 2, kReconnectMaxDelayMs);
    m_reconnectAttempts++;
}

void TakServerConnection::connectToServer()
{
    QMutexLocker lock(&m_mutex);

    m_reconnectTimer->stop();
    m_userDisconnected = false;

    if (m_connected) {
        return;
    }

    if (m_config.address.isEmpty()) {
        m_statusText = "No server address configured";
        emit statusChanged(m_statusText);
        emit connectionError(m_statusText);
        return;
    }

    m_socket = new QSslSocket(this);
    m_readBuffer.clear();

    setupSsl();

    connect(m_socket, &QSslSocket::connected, this, &TakServerConnection::onConnected);
    connect(m_socket, &QSslSocket::disconnected, this, &TakServerConnection::onDisconnected);
    connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred),
            this, QOverload<QAbstractSocket::SocketError>::of(&TakServerConnection::onError));
    connect(m_socket, QOverload<const QList<QSslError>&>::of(&QSslSocket::sslErrors),
            this, QOverload<const QList<QSslError>&>::of(&TakServerConnection::onSslErrors));
    connect(m_socket, &QSslSocket::readyRead, this, &TakServerConnection::onReadyRead);

    m_statusText = QString("Connecting to %1:%2...").arg(m_config.address).arg(m_config.port);
    emit statusChanged(m_statusText);

    m_socket->connectToHostEncrypted(m_config.address, m_config.port);
}

void TakServerConnection::disconnect()
{
    QMutexLocker lock(&m_mutex);

    m_reconnectTimer->stop();
    m_reconnectAttempts = 0;
    m_reconnectDelayMs = kReconnectMinDelayMs;
    m_userDisconnected = true;

    if (m_socket) {
        m_socket->disconnectFromHost();
        if (m_socket->state() != QAbstractSocket::UnconnectedState) {
            m_socket->waitForDisconnected(1000);
        }
        m_socket->deleteLater();
        m_socket = nullptr;
    }

    m_connected = false;
    m_readBuffer.clear();
    m_statusText = "Disconnected";
    emit statusChanged(m_statusText);
    emit disconnected();
}

void TakServerConnection::sendCoT(const QString& xml)
{
    QMutexLocker lock(&m_mutex);

    if (!m_connected || !m_socket) {
        emit connectionError("Not connected to TAK server");
        return;
    }

    QByteArray data = xml.toUtf8();
    data.append('\n');
    m_socket->write(data);
    m_socket->flush();
}

QString TakServerConnection::statusText() const
{
    return m_statusText;
}

void TakServerConnection::setupSsl()
{
    QSslConfiguration sslConfig = QSslConfiguration::defaultConfiguration();
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);

    if (!m_config.certFilePath.isEmpty()) {
        QFileInfo fi(m_config.certFilePath);
        if (fi.exists()) {
            QFile certFile(m_config.certFilePath);
            if (certFile.open(QIODevice::ReadOnly)) {
                QByteArray pfxData = certFile.readAll();
                certFile.close();

                QSslCertificate cert;
                QSslKey key;
                QList<QSslCertificate> extraCerts;

                QBuffer buffer(&pfxData);
                buffer.open(QIODevice::ReadOnly);

                static bool legacyLoaded = false;
                if (!legacyLoaded) {
                    OSSL_PROVIDER* legacy = OSSL_PROVIDER_load(NULL, "legacy");
                    if (legacy) {
                        legacyLoaded = true;
                        qDebug() << "TAK: OpenSSL legacy provider loaded";
                    }
                }

                bool imported = false;
                if (!m_config.certPassword.isEmpty()) {
                    imported = QSslCertificate::importPkcs12(&buffer, &key, &cert, &extraCerts, m_config.certPassword.toUtf8());
                } else {
                    imported = QSslCertificate::importPkcs12(&buffer, &key, &cert, &extraCerts);
                }

                buffer.close();

                if (imported) {
                    sslConfig.setLocalCertificate(cert);
                    sslConfig.setPrivateKey(key);
                    for (const QSslCertificate& caCert : extraCerts) {
                        sslConfig.addCaCertificate(caCert);
                    }
                    m_statusText = "Certificate loaded";
                    emit statusChanged(m_statusText);
                } else {
                    m_statusText = QString("Failed to load certificate: %1").arg(m_config.certPassword.isEmpty() ? "wrong format or no password needed" : "wrong password or legacy cipher (OpenSSL 3)");
                    qWarning() << "TAK: PKCS12 import failed -" << m_statusText;
                    emit statusChanged(m_statusText);
                    emit connectionError(m_statusText);
                }
            } else {
                m_statusText = "Cannot open certificate file";
                emit statusChanged(m_statusText);
                emit connectionError(m_statusText);
            }
        } else {
            m_statusText = "Certificate file not found";
            emit statusChanged(m_statusText);
            emit connectionError(m_statusText);
        }
    }

    m_socket->setSslConfiguration(sslConfig);
}

void TakServerConnection::parseBuffer()
{
    while (true) {
        int openPos = m_readBuffer.indexOf("<event");
        if (openPos == -1) {
            int discardPos = m_readBuffer.indexOf('>');
            if (discardPos != -1) {
                m_readBuffer = m_readBuffer.mid(discardPos + 1);
            }
            break;
        }

        if (openPos > 0) {
            m_readBuffer = m_readBuffer.mid(openPos);
        }

        int closePos = m_readBuffer.indexOf("</event>");
        if (closePos == -1) {
            break;
        }

        closePos += 8;
        QByteArray rawMessage = m_readBuffer.mid(0, closePos);
        m_readBuffer = m_readBuffer.mid(closePos);

        QString xml = QString::fromUtf8(rawMessage).trimmed();
        if (CoTMessageParser::isValid(xml)) {
            emit messageReceived(xml);
        } else {
            qDebug() << "TAK Connection: Ignoring invalid CoT from" << m_config.name << ":" << xml.left(200);
        }
    }
}

void TakServerConnection::onConnected()
{
    m_connected = true;
    m_reconnectAttempts = 0;
    m_reconnectDelayMs = kReconnectMinDelayMs;
    m_reconnectTimer->stop();
    m_statusText = QString("Connected to %1 as %2").arg(m_config.address).arg(m_config.callsign);
    emit statusChanged(m_statusText);
    emit connected();
}

void TakServerConnection::onDisconnected()
{
    m_connected = false;
    m_statusText = "Disconnected";
    emit statusChanged(m_statusText);
    emit disconnected();

    scheduleReconnect();
}

void TakServerConnection::onError(QAbstractSocket::SocketError socketError)
{
    Q_UNUSED(socketError);
    m_connected = false;

    if (m_socket) {
        m_socket->deleteLater();
        m_socket = nullptr;
    }

    m_statusText = QString("Connection error: %1").arg(m_socket ? m_socket->errorString() : "Unknown error");
    emit statusChanged(m_statusText);
    emit connectionError(m_statusText);
    emit disconnected();

    scheduleReconnect();
}

void TakServerConnection::onSslErrors(const QList<QSslError>& errors)
{
    for (const QSslError& err : errors) {
        qDebug() << "TAK SSL Error:" << err.errorString();
    }
    m_socket->ignoreSslErrors();
}

void TakServerConnection::onReadyRead()
{
    m_readBuffer.append(m_socket->readAll());
    parseBuffer();
}

void TakServerConnection::onReconnectTimeout()
{
    connectToServer();
}

QString TakServerConnection::generateUid() const
{
    return m_uid;
}
