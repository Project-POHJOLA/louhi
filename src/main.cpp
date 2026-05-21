#include <QApplication>
#include <QDir>
#include <QDebug>
#include <QDockWidget>
#include "mainwindow.h"
#include "plugininterface.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("LOUHI");
    app.setApplicationVersion("0.1");

    QString pluginPath = QCoreApplication::applicationDirPath() + "/plugins";
    qDebug() << "Plugin directory:" << pluginPath;

    MainWindow window;
    QMap<PluginInterface*, QDockWidget*> pluginDocks;

    QDir dir(pluginPath);
    if (dir.exists()) {
        window.getPluginManager()->discoverPlugins(pluginPath);
        window.getPluginManager()->loadAllPlugins();

        for (PluginInterface* plugin : window.getPluginManager()->getEnabledPlugins()) {
            if (plugin->initialize()) {
                if (plugin->start()) {
                    PluginInfo info = plugin->getPluginInfo();
                    if (info.type == PluginType::Screen) {
                        QWidget* widget = plugin->getWidget();
                        if (widget) {
                            QDockWidget* dock = new QDockWidget(info.name, &window);
                            dock->setWidget(widget);
                            dock->setAttribute(Qt::WA_DeleteOnClose, false);
                            window.addDockWidget(Qt::RightDockWidgetArea, dock);
                            pluginDocks[plugin] = dock;

                            QObject::connect(plugin, &PluginInterface::showWidgetRequested,
                                [dock]() {
                                    dock->show();
                                    dock->raise();
                                });
                        }
                    }
                }
            }
        }

        window.getPluginManager()->setupMenu(window.menuBar());
        window.setupConnectionLeds();

        QAction* pluginMgrAction = window.menuBar()->addAction("Plugin Manager");
        QObject::connect(pluginMgrAction, &QAction::triggered, &window, &MainWindow::showPluginManager);
    } else {
        qWarning() << "Plugin directory not found:" << pluginPath;
    }

    window.show();

    return app.exec();
}