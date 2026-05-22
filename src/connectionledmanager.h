#ifndef CONNECTIONLEDMANAGER_H
#define CONNECTIONLEDMANAGER_H

#include <QObject>
#include <QMap>
#include <QStatusBar>
#include "connectionstatusled.h"

class ConnectionLedManager : public QObject
{
    Q_OBJECT

public:
    explicit ConnectionLedManager(QStatusBar* statusBar, QObject* parent = nullptr);

    void addLed(const QString& connectionId, const QString& connectionType, const QString& connectionName);
    void removeLed(const QString& connectionId);
    void clearAllLeds();
    void setLedState(const QString& connectionId, ConnectionStatusLed::ConnectionState state);

public slots:
    void onPluginStatusChanged(const QString& pluginId, const QString& status);
    void onPluginMessageReceived(const QString& pluginId, const QString& topic, const QString& payload);

private:
    struct LedInfo {
        ConnectionStatusLed* led;
        QString connectionType;
        QString connectionName;
        bool configured;
    };

    QStatusBar* m_statusBar;
    QMap<QString, LedInfo> m_leds;
};

#endif
