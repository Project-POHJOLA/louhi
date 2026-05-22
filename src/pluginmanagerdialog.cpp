#include "pluginmanagerdialog.h"
#include "pluginmanager.h"
#include "plugininterface.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QHeaderView>
#include <QDialog>
#include <QFormLayout>

PluginManagerDialog::PluginManagerDialog(PluginManager* pluginManager, QWidget* parent)
    : QDialog(parent)
    , m_pluginManager(pluginManager)
{
    setWindowTitle(tr("Plugin Manager"));
    setMinimumSize(600, 400);

    QVBoxLayout* layout = new QVBoxLayout(this);

    QLabel* title = new QLabel(tr("Manage Plugins"), this);
    title->setStyleSheet("font-weight: bold; font-size: 14pt;");
    layout->addWidget(title);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(4);
    m_table->setHorizontalHeaderLabels({tr("Name"), tr("Version"), tr("Type"), tr("Enabled")});
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    layout->addWidget(m_table);

    QHBoxLayout* buttonLayout = new QHBoxLayout();
    QPushButton* enableBtn = new QPushButton(tr("Enable"), this);
    QPushButton* disableBtn = new QPushButton(tr("Disable"), this);
    QPushButton* infoBtn = new QPushButton(tr("Info"), this);
    QPushButton* closeBtn = new QPushButton(tr("Close"), this);

    buttonLayout->addWidget(enableBtn);
    buttonLayout->addWidget(disableBtn);
    buttonLayout->addWidget(infoBtn);
    buttonLayout->addStretch();
    buttonLayout->addWidget(closeBtn);

    layout->addLayout(buttonLayout);

    connect(enableBtn, &QPushButton::clicked, this, &PluginManagerDialog::enableSelected);
    connect(disableBtn, &QPushButton::clicked, this, &PluginManagerDialog::disableSelected);
    connect(infoBtn, &QPushButton::clicked, this, &PluginManagerDialog::showInfo);
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

void PluginManagerDialog::showInfo()
{
    int row = m_table->currentRow();
    if (row < 0)
        return;

    QString id = m_table->item(row, 0)->data(Qt::UserRole).toString();
    for (PluginInterface* plugin : m_pluginManager->getAllPlugins()) {
        PluginInfo info = plugin->getPluginInfo();
        if (info.id == id) {
            QDialog* dialog = new QDialog(this);
            dialog->setWindowTitle(info.name);
            dialog->setMinimumWidth(350);

            QVBoxLayout* layout = new QVBoxLayout(dialog);
            QFormLayout* form = new QFormLayout();

            QLabel* authorLabel = new QLabel(info.author, dialog);
            authorLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
            form->addRow(tr("Author:"), authorLabel);

            QLabel* descLabel = new QLabel(info.description, dialog);
            descLabel->setWordWrap(true);
            descLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
            form->addRow(tr("Description:"), descLabel);

            layout->addLayout(form);

            QPushButton* closeBtn = new QPushButton(tr("Close"), dialog);
            connect(closeBtn, &QPushButton::clicked, dialog, &QDialog::accept);
            layout->addWidget(closeBtn, 0, Qt::AlignRight);

            dialog->exec();
            dialog->deleteLater();
            break;
        }
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
            case PluginType::Communication: typeStr = tr("Communication"); break;
            case PluginType::Map: typeStr = tr("Map"); break;
            case PluginType::Screen: typeStr = tr("Screen"); break;
        }
        m_table->setItem(row, 2, new QTableWidgetItem(typeStr));

        QTableWidgetItem* enabledItem = new QTableWidgetItem(
            m_pluginManager->getEnabledPlugins().contains(plugin) ? tr("Yes") : tr("No"));
        m_table->setItem(row, 3, enabledItem);
    }
}