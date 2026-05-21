#include "pluginmanagerdialog.h"
#include "pluginmanager.h"
#include "plugininterface.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QHeaderView>

PluginManagerDialog::PluginManagerDialog(PluginManager* pluginManager, QWidget* parent)
    : QDialog(parent)
    , m_pluginManager(pluginManager)
{
    setWindowTitle("Plugin Manager");
    setMinimumSize(600, 400);

    QVBoxLayout* layout = new QVBoxLayout(this);

    QLabel* title = new QLabel("Manage Plugins", this);
    title->setStyleSheet("font-weight: bold; font-size: 14pt;");
    layout->addWidget(title);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(4);
    m_table->setHorizontalHeaderLabels({"Name", "Version", "Type", "Enabled"});
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    layout->addWidget(m_table);

    QHBoxLayout* buttonLayout = new QHBoxLayout();
    QPushButton* enableBtn = new QPushButton("Enable", this);
    QPushButton* disableBtn = new QPushButton("Disable", this);
    QPushButton* closeBtn = new QPushButton("Close", this);

    buttonLayout->addWidget(enableBtn);
    buttonLayout->addWidget(disableBtn);
    buttonLayout->addStretch();
    buttonLayout->addWidget(closeBtn);

    layout->addLayout(buttonLayout);

    connect(enableBtn, &QPushButton::clicked, this, &PluginManagerDialog::enableSelected);
    connect(disableBtn, &QPushButton::clicked, this, &PluginManagerDialog::disableSelected);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);

    loadPlugins();
}

PluginManagerDialog::~PluginManagerDialog()
{
}

void PluginManagerDialog::enableSelected()
{
    int row = m_table->currentRow();
    if (row >= 0) {
        QString id = m_table->item(row, 0)->data(Qt::UserRole).toString();
        m_pluginManager->enablePlugin(id);
        loadPlugins();
    }
}

void PluginManagerDialog::disableSelected()
{
    int row = m_table->currentRow();
    if (row >= 0) {
        QString id = m_table->item(row, 0)->data(Qt::UserRole).toString();
        m_pluginManager->disablePlugin(id);
        loadPlugins();
    }
}

void PluginManagerDialog::loadPlugins()
{
    m_table->setRowCount(0);

    for (PluginInterface* plugin : m_pluginManager->getAllPlugins()) {
        PluginInfo info = plugin->getPluginInfo();
        int row = m_table->rowCount();
        m_table->insertRow(row);

        QTableWidgetItem* nameItem = new QTableWidgetItem(info.name);
        nameItem->setData(Qt::UserRole, info.id);
        m_table->setItem(row, 0, nameItem);

        m_table->setItem(row, 1, new QTableWidgetItem(info.version));

        QString typeStr;
        switch (info.type) {
            case PluginType::Communication: typeStr = "Communication"; break;
            case PluginType::Map: typeStr = "Map"; break;
            case PluginType::Screen: typeStr = "Screen"; break;
        }
        m_table->setItem(row, 2, new QTableWidgetItem(typeStr));

        QTableWidgetItem* enabledItem = new QTableWidgetItem(
            m_pluginManager->getEnabledPlugins().contains(plugin) ? "Yes" : "No");
        m_table->setItem(row, 3, enabledItem);
    }
}