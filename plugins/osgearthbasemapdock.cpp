#include "osgearthbasemapdock.h"
#include <QVBoxLayout>
#include <QDebug>

static MapSource osmSource()
{
    MapSource osm;
    osm.name = "OSM Standard";
    osm.type = "xyz";
    osm.url = "https://tile.openstreetmap.org/{z}/{x}/{y}.png";
    osm.maxZoom = 19;
    osm.builtIn = true;
    return osm;
}

static MapSource cartoDarkSource()
{
    MapSource dark;
    dark.name = "Carto Dark";
    dark.type = "xyz";
    dark.url = "https://a.basemaps.cartocdn.com/dark_all/{z}/{x}/{y}.png";
    dark.maxZoom = 19;
    dark.builtIn = true;
    return dark;
}

BasemapDockWidget::BasemapDockWidget(QWidget* parent)
    : QDockWidget(parent)
    , m_listWidget(nullptr)
    , m_activeSourceName("OSM Standard")
{
    setWindowTitle(tr("Basemaps"));
    setMinimumWidth(200);

    m_listWidget = new QListWidget(this);
    setWidget(m_listWidget);

    connect(m_listWidget, &QListWidget::currentItemChanged, this,
        [this](QListWidgetItem* current, QListWidgetItem* /*previous*/) {
            if (!current) return;
            QString name = current->data(Qt::UserRole).toString();
            for (const MapSource& src : allSources()) {
                if (src.name == name) {
                    m_activeSourceName = name;
                    emit sourceSelected(src);
                    break;
                }
            }
        });
}

QList<MapSource> BasemapDockWidget::allSources() const
{
    QList<MapSource> sources;
    sources.append(osmSource());
    sources.append(cartoDarkSource());
    sources.append(m_customSources);
    return sources;
}

void BasemapDockWidget::setSources(const QList<MapSource>& customSources)
{
    m_customSources = customSources;
    refreshList();
}

void BasemapDockWidget::setActiveSource(const QString& sourceName)
{
    m_activeSourceName = sourceName;
    for (int i = 0; i < m_listWidget->count(); ++i) {
        QListWidgetItem* item = m_listWidget->item(i);
        if (item->data(Qt::UserRole).toString() == sourceName) {
            m_listWidget->blockSignals(true);
            item->setSelected(true);
            m_listWidget->blockSignals(false);
            break;
        }
    }
}

void BasemapDockWidget::refreshList()
{
    m_listWidget->blockSignals(true);
    m_listWidget->clear();

    QList<MapSource> sources = allSources();
    for (int i = 0; i < sources.size(); ++i) {
        const MapSource& src = sources[i];
        QListWidgetItem* item = new QListWidgetItem(src.name);
        item->setData(Qt::UserRole, src.name);
        item->setData(Qt::UserRole + 1, src.builtIn ? "builtin" : "custom");
        if (src.name == m_activeSourceName) {
            item->setSelected(true);
        }
        m_listWidget->addItem(item);
    }

    m_listWidget->blockSignals(false);
}
