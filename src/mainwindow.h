#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QDockWidget>
#include <QAction>
#include <QActionGroup>
#include <QLabel>
#include "pluginmanager.h"
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

    bool restoreDockState();
    void setupToolbar();

public slots:
    void showPluginManager();
    void showAbout();
    void onPluginConfigChanged(const QString& pluginId);
    void setupConnectionLeds();
    void setupLanguageMenu();
    void changeLanguage(const QString& langCode);
    void toggleEmcon();
    void setEmconToggleAction(QAction* action);

private:
    void clearPluginToolbars();

    PluginManager* m_pluginManager;
    ConfigManager* m_configManager;
    ConnectionLedManager* m_ledManager;
    QToolBar* m_mainToolBar;
    QList<QToolBar*> m_pluginToolbars;
    QByteArray m_pendingDockState;
    QLabel* m_emconIndicator;
    QAction* m_emconToggleAction;
};

#endif