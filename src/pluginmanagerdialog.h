#ifndef PLUGINMANAGERDIALOG_H
#define PLUGINMANAGERDIALOG_H

#include <QDialog>
#include <QTableWidget>

class PluginManager;

class PluginManagerDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PluginManagerDialog(PluginManager* pluginManager, QWidget* parent = nullptr);
    ~PluginManagerDialog();

private slots:
    void enableSelected();
    void disableSelected();
    void loadPlugins();

private:
    PluginManager* m_pluginManager;
    QTableWidget* m_table;
};

#endif