#include "mapsourcesdialog.h"
#include "mapsources.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QListWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QGroupBox>
#include <QInputDialog>
#include <QMessageBox>
#include <QDialogButtonBox>
#include <QDebug>

MapSourcesDialog::MapSourcesDialog(const QList<MapSource>& customSources,
                                   QWidget* parent)
    : QDialog(parent)
    , m_customSources(customSources)
{
    setWindowTitle(tr("Map Sources"));
    setMinimumSize(500, 400);
    buildUi();
}

QList<MapSource> MapSourcesDialog::customSources() const
{
    return m_customSources;
}

void MapSourcesDialog::buildUi()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    QLabel* infoLabel = new QLabel(
        tr("Manage custom tile sources. Built-in sources (OSM Standard, Carto Dark)\n"
        "are always available and can be selected from the basemap panel."), this);
    infoLabel->setWordWrap(true);
    mainLayout->addWidget(infoLabel);

    m_sourceList = new QListWidget(this);
    mainLayout->addWidget(m_sourceList);

    QHBoxLayout* btnLayout = new QHBoxLayout();
    m_addXyzBtn = new QPushButton(tr("Add XYZ Source"), this);
    m_addWmsBtn = new QPushButton(tr("Add WMS Source"), this);
    m_removeBtn = new QPushButton(tr("Remove"), this);
    m_removeBtn->setEnabled(false);
    btnLayout->addWidget(m_addXyzBtn);
    btnLayout->addWidget(m_addWmsBtn);
    btnLayout->addStretch();
    btnLayout->addWidget(m_removeBtn);
    mainLayout->addLayout(btnLayout);

    QDialogButtonBox* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    mainLayout->addWidget(buttons);

    connect(m_addXyzBtn, &QPushButton::clicked, this, &MapSourcesDialog::onAddXyz);
    connect(m_addWmsBtn, &QPushButton::clicked, this, &MapSourcesDialog::onAddWms);
    connect(m_removeBtn, &QPushButton::clicked, this, &MapSourcesDialog::onRemove);
    connect(m_sourceList, &QListWidget::currentRowChanged, this, [this]() {
        m_removeBtn->setEnabled(m_sourceList->currentItem() != nullptr);
    });
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    refreshList();
}

void MapSourcesDialog::refreshList()
{
    m_sourceList->clear();

    for (int i = 0; i < m_customSources.size(); ++i) {
        const MapSource& src = m_customSources[i];
        QString label = QString("%1 [%2]").arg(src.name, src.type.toUpper());
        QListWidgetItem* item = new QListWidgetItem(label);
        item->setData(Qt::UserRole, src.name);
        item->setData(Qt::UserRole + 1, "custom");
        item->setData(Qt::UserRole + 2, i);
        m_sourceList->addItem(item);
    }
}

bool MapSourcesDialog::validateXyzInput(const QString& url) const
{
    return url.contains("{z}") && url.contains("{x}") && url.contains("{y}");
}

void MapSourcesDialog::onAddXyz()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Add XYZ Tile Source"));
    QFormLayout form(&dialog);

    QLineEdit* nameEdit = new QLineEdit(&dialog);
    nameEdit->setPlaceholderText(tr("My XYZ Source"));
    form.addRow(tr("Name:"), nameEdit);

    QLineEdit* urlEdit = new QLineEdit(&dialog);
    urlEdit->setPlaceholderText(tr("https://example.com/tiles/{z}/{x}/{y}.png"));
    form.addRow(tr("URL Template:"), urlEdit);

    QLineEdit* maxZoomEdit = new QLineEdit(&dialog);
    maxZoomEdit->setText("19");
    form.addRow(tr("Max Zoom:"), maxZoomEdit);

    QDialogButtonBox* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    form.addRow(buttons);

    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        QString name = nameEdit->text().trimmed();
        QString url = urlEdit->text().trimmed();
        int maxZoom = maxZoomEdit->text().toInt();

        if (name.isEmpty() || url.isEmpty()) {
            QMessageBox::warning(this, tr("Invalid Input"),
                                 tr("Name and URL template are required."));
            return;
        }

        if (!validateXyzInput(url)) {
            QMessageBox::warning(this, tr("Invalid URL"),
                                 tr("URL must contain {z}, {x}, and {y} placeholders."));
            return;
        }

        MapSource src;
        src.name = name;
        src.type = "xyz";
        src.url = url;
        src.maxZoom = qBound(1, maxZoom, 22);
        m_customSources.append(src);
        refreshList();
    }
}

void MapSourcesDialog::onAddWms()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Add WMS Tile Source"));
    QFormLayout form(&dialog);

    QLineEdit* nameEdit = new QLineEdit(&dialog);
    nameEdit->setPlaceholderText(tr("My WMS Source"));
    form.addRow(tr("Name:"), nameEdit);

    QLineEdit* urlEdit = new QLineEdit(&dialog);
    urlEdit->setPlaceholderText(tr("http://example.com/wms"));
    form.addRow(tr("Base URL:"), urlEdit);

    QLineEdit* layersEdit = new QLineEdit(&dialog);
    layersEdit->setPlaceholderText(tr("layer1,layer2"));
    form.addRow(tr("Layers:"), layersEdit);

    QLineEdit* formatEdit = new QLineEdit(&dialog);
    formatEdit->setText("image/png");
    form.addRow(tr("Format:"), formatEdit);

    QLineEdit* crsEdit = new QLineEdit(&dialog);
    crsEdit->setText("EPSG:3857");
    form.addRow(tr("CRS:"), crsEdit);

    QLineEdit* stylesEdit = new QLineEdit(&dialog);
    form.addRow(tr("Styles:"), stylesEdit);

    QLineEdit* maxZoomEdit = new QLineEdit(&dialog);
    maxZoomEdit->setText("18");
    form.addRow(tr("Max Zoom:"), maxZoomEdit);

    QDialogButtonBox* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    form.addRow(buttons);

    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        QString name = nameEdit->text().trimmed();
        QString url = urlEdit->text().trimmed();
        QString layers = layersEdit->text().trimmed();

        if (name.isEmpty() || url.isEmpty() || layers.isEmpty()) {
            QMessageBox::warning(this, tr("Invalid Input"),
                                 tr("Name, Base URL, and Layers are required."));
            return;
        }

        MapSource src;
        src.name = name;
        src.type = "wms";
        src.url = url;
        src.layers = layers;
        src.format = formatEdit->text().trimmed().isEmpty() ? "image/png" : formatEdit->text().trimmed();
        src.crs = crsEdit->text().trimmed().isEmpty() ? "EPSG:3857" : crsEdit->text().trimmed();
        src.styles = stylesEdit->text().trimmed();
        src.maxZoom = qBound(1, maxZoomEdit->text().toInt(), 22);
        m_customSources.append(src);
        refreshList();
    }
}

void MapSourcesDialog::onRemove()
{
    QListWidgetItem* item = m_sourceList->currentItem();
    if (!item) return;

    int idx = item->data(Qt::UserRole + 2).toInt();
    if (idx >= 0 && idx < m_customSources.size()) {
        m_customSources.removeAt(idx);
        refreshList();
    }
}
