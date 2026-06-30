#ifndef ENTITYINFOWIDGET_H
#define ENTITYINFOWIDGET_H

#include <QDockWidget>
#include <QTextBrowser>
#include <QString>
#include "osgearthmapwidget.h"

class EntityInfoWidget : public QDockWidget
{
    Q_OBJECT

public:
    explicit EntityInfoWidget(QWidget* parent = nullptr);

public slots:
    void showEntity(const MapEntity& entity);
    void clear();

private:
    QString formatHtml(const MapEntity& entity) const;
    QString formatPosition(double lat, double lon) const;
    QString mgrsFromLatLon(double lat, double lon) const;
    QString formatRemarks(const QString& raw) const;
    QString formatBatteryBar(int percent) const;
    QString formatSpeedKmph(double speedMs) const;
    QString linkLabelForUid(const QString& uid) const;
    QString parseDetail(const QString& detailXml, const QString& element) const;
    QString parseAttr(const QString& detailXml, const QString& element,
                      const QString& attr) const;

    QTextBrowser* m_browser;
};

#endif
