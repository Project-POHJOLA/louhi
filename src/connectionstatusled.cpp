#include "connectionstatusled.h"
#include <QPainter>

ConnectionStatusLed::ConnectionStatusLed(const QString& connectionType, const QString& connectionName, QWidget* parent)
    : QWidget(parent)
    , m_connectionType(connectionType)
    , m_connectionName(connectionName)
    , m_state(Disconnected)
    , m_trafficTimer(new QTimer(this))
{
    setFixedSize(16, 16);
    m_trafficTimer->setSingleShot(true);
    m_trafficTimer->setInterval(250);
    connect(m_trafficTimer, &QTimer::timeout, this, [this]() {
        if (m_state == Traffic) {
            m_state = Connected;
            update();
        }
    });
    updateTooltip();
}

void ConnectionStatusLed::setConnectionState(ConnectionState state)
{
    if (state == Traffic) {
        m_trafficTimer->start();
    }
    m_state = state;
    update();
    updateTooltip();
}

void ConnectionStatusLed::updateTooltip()
{
    QString statusText;
    switch (m_state) {
        case Disconnected:
            statusText = tr("Disconnected");
            break;
        case Connected:
            statusText = tr("Connected");
            break;
        case Traffic:
            statusText = tr("Traffic");
            break;
    }
    setToolTip(QString("%1:%2 - %3").arg(m_connectionType).arg(m_connectionName).arg(statusText));
}

void ConnectionStatusLed::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QRectF rect = QRectF(2, 2, 12, 12).translated(0, 0);

    QColor baseColor;
    QColor highlightColor;
    QColor shadowColor;

    switch (m_state) {
        case Disconnected:
            baseColor = QColor(180, 30, 30);
            highlightColor = QColor(220, 80, 80);
            shadowColor = QColor(120, 20, 20);
            break;
        case Connected:
            baseColor = QColor(40, 100, 40);
            highlightColor = QColor(70, 140, 70);
            shadowColor = QColor(25, 60, 25);
            break;
        case Traffic:
            baseColor = QColor(50, 200, 50);
            highlightColor = QColor(120, 255, 120);
            shadowColor = QColor(30, 140, 30);
            break;
    }

    painter.setBrush(QColor(30, 30, 30));
    painter.setPen(QPen(QColor(60, 60, 60), 1));
    painter.drawEllipse(rect);

    QRectF innerRect = QRectF(4, 4, 8, 8);
    QRadialGradient gradient(innerRect.center(), 6);
    gradient.setColorAt(0.0, highlightColor);
    gradient.setColorAt(0.5, baseColor);
    gradient.setColorAt(1.0, shadowColor);

    painter.setBrush(gradient);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(innerRect);
}
