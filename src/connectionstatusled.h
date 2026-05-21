#ifndef CONNECTIONSTATUSLED_H
#define CONNECTIONSTATUSLED_H

#include <QWidget>
#include <QTimer>

class ConnectionStatusLed : public QWidget
{
    Q_OBJECT

public:
    enum ConnectionState {
        Disconnected,
        Connected,
        Traffic
    };

    explicit ConnectionStatusLed(const QString& connectionType, const QString& connectionName, QWidget* parent = nullptr);

    void setConnectionState(ConnectionState state);
    ConnectionState connectionState() const { return m_state; }

    QString connectionType() const { return m_connectionType; }
    QString connectionName() const { return m_connectionName; }

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void updateTooltip();

    QString m_connectionType;
    QString m_connectionName;
    ConnectionState m_state;
    QTimer* m_trafficTimer;
};

#endif
