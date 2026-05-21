#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QDockWidget>
#include <QAction>
#include "pluginmanager.h"
#include "configmanager.h"
#include "pluginmanagerdialog.h"
#include "connectionledmanager.h"

class PluginInterface;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

    ConfigManager* getConfigManager() const { return m_configManager; }
    PluginManager* getPluginManager() const { return m_pluginManager; }

public slots:
    void showPluginManager();
    void setupConnectionLeds();

private:
    PluginManager* m_pluginManager;
    ConfigManager* m_configManager;
    ConnectionLedManager* m_ledManager;
    QAction* m_pluginManagerAction;
};

#endif