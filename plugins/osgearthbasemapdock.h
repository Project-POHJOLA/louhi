#ifndef OSGEARTHBASEMAPDOCK_H
#define OSGEARTHBASEMAPDOCK_H

#include <QDockWidget>
#include <QListWidget>
#include <QList>
#include "mapsources.h"

class OsgEarthPlugin;

class BasemapDockWidget : public QDockWidget
{
    Q_OBJECT

public:
    explicit BasemapDockWidget(QWidget* parent = nullptr);

    void setSources(const QList<MapSource>& customSources);
    void setActiveSource(const QString& sourceName);

signals:
    void sourceSelected(const MapSource& source);

private:
    void refreshList();
    QList<MapSource> allSources() const;

    QListWidget* m_listWidget;
    QList<MapSource> m_customSources;
    QString m_activeSourceName;
};

#endif
