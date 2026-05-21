#include "connectionledmanager.h"
#include <QHBoxLayout>

ConnectionLedManager::ConnectionLedManager(QStatusBar* statusBar, QObject* parent)
    : QObject(parent)
    , m_statusBar(statusBar)
{
}

void ConnectionLedManager::addLed(const QString& connectionId, const QString& connectionType, const QString& connectionName)
{
    if (m_leds.contains(connectionId)) {
        return;
    }

    LedInfo info;
    info.led = new ConnectionStatusLed(connectionType, connectionName);
    info.connectionType = connectionType;
    info.connectionName = connectionName;
    info.configured = false;

    m_leds[connectionId] = info;

    QWidget* container = new QWidget();
    QHBoxLayout* layout = new QHBoxLayout(container);
    layout->setContentsMargins(4, 0, 4, 0);
    layout->addWidget(info.led);
    container->setLayout(layout);

    m_statusBar->addPermanentWidget(container);
}

void ConnectionLedManager::removeLed(const QString& connectionId)
{
    if (!m_leds.contains(connectionId)) {
        return;
    }

    LedInfo& info = m_leds[connectionId];
    info.led->parentWidget()->deleteLater();
    m_leds.remove(connectionId);
}

void ConnectionLedManager::setLedState(const QString& connectionId, ConnectionStatusLed::ConnectionState state)
{
    if (!m_leds.contains(connectionId)) {
        return;
    }

    m_leds[connectionId].led->setConnectionState(state);
}

void ConnectionLedManager::onPluginStatusChanged(const QString& pluginId, const QString& status)
{
    int colonPos = status.lastIndexOf(':');
    if (colonPos == -1) return;

    QString connectionName = status.left(colonPos);
    QString state = status.mid(colonPos + 1);

    ConnectionStatusLed::ConnectionState ledState;
    if (state == "Connected") {
        ledState = ConnectionStatusLed::Connected;
    } else if (state == "Disconnected") {
        ledState = ConnectionStatusLed::Disconnected;
    } else {
        return;
    }

    if (pluginId == "nats_communication") {
        setLedState("nats_communication", ledState);
    } else if (pluginId == "tak_communication") {
        QString ledId = "tak_communication_" + connectionName;
        setLedState(ledId, ledState);
    }
}

void ConnectionLedManager::onPluginMessageReceived(const QString& pluginId, const QString& topic, const QString& payload)
{
    Q_UNUSED(topic);
    Q_UNUSED(payload);

    if (pluginId == "nats_communication") {
        setLedState("nats_communication", ConnectionStatusLed::Traffic);
    } else if (pluginId == "tak_communication") {
        for (auto it = m_leds.begin(); it != m_leds.end(); ++it) {
            if (it.key().startsWith("tak_communication_")) {
                it.value().led->setConnectionState(ConnectionStatusLed::Traffic);
            }
        }
    }
}
