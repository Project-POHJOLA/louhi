#include <QApplication>
#include <QDir>
#include <QDebug>
#include <QDockWidget>
#include <QIcon>
#include <QTranslator>
#include <QLibraryInfo>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include "mainwindow.h"
#include "configmanager.h"
#include "plugininterface.h"

static QString translationDir()
{
    QStringList dirs = {
        QCoreApplication::applicationDirPath() + "/translations",
        QCoreApplication::applicationDirPath() + "/../translations",
        QCoreApplication::applicationDirPath() + "/../share/louhi/translations",
        "translations"
    };
    for (const QString& d : dirs) {
        if (QDir(d).exists()) return d;
    }
    return QString();
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("LOUHI");
    app.setApplicationVersion("0.1");
    app.setWindowIcon(QIcon(":/assets/louhi_icon.png"));

    QTranslator qtTranslator;
    qtTranslator.load("qt_" + QLocale::system().name(),
                      QLibraryInfo::location(QLibraryInfo::TranslationsPath));
    app.installTranslator(&qtTranslator);

    QTranslator appTranslator;
    QString lang;
    QFile configFile(ConfigManager::defaultConfigPath());
    if (configFile.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(configFile.readAll());
        QJsonObject appCfg = doc.object().value("app").toObject();
        lang = appCfg.value("language").toString();
        configFile.close();
    }

    QString tDir = translationDir();
    if (!lang.isEmpty()) {
        appTranslator.load("louhi_" + lang, tDir);
    } else {
        appTranslator.load(QLocale(), "louhi", "_", tDir);
    }
    app.installTranslator(&appTranslator);

    QString pluginPath = QCoreApplication::applicationDirPath() + "/plugins";
    qDebug() << "Plugin directory:" << pluginPath;

    MainWindow window;
    QMap<PluginInterface*, QDockWidget*> pluginDocks;

    QDir dir(pluginPath);
    if (dir.exists()) {
        window.getPluginManager()->discoverPlugins(pluginPath);
        window.getPluginManager()->loadAllPlugins();

        QDockWidget* mapDock = nullptr;

        for (PluginInterface* plugin : window.getPluginManager()->getEnabledPlugins()) {
            PluginInfo info = plugin->getPluginInfo();
            QJsonObject pluginConfig = window.getConfigManager()->getPluginConfig(info.id);
            plugin->setConfig(pluginConfig);
            if (plugin->initialize()) {
                if (plugin->start()) {
                    if (info.type == PluginType::Screen || info.type == PluginType::Map) {
                        QWidget* widget = plugin->getWidget();
                        if (widget) {
                            QDockWidget* dock = new QDockWidget(info.name, &window);
                            dock->setObjectName(info.name);
                            dock->setWidget(widget);
                            dock->setAttribute(Qt::WA_DeleteOnClose, false);

                            if (info.type == PluginType::Map) {
                                window.addDockWidget(Qt::RightDockWidgetArea, dock);
                                mapDock = dock;
                            } else if (mapDock) {
                                window.splitDockWidget(mapDock, dock, Qt::Horizontal);
                            } else {
                                window.addDockWidget(Qt::RightDockWidgetArea, dock);
                            }

                            QObject::connect(plugin, &PluginInterface::showWidgetRequested,
                        [dock]() {
                            dock->show();
                            dock->raise();
                        });
                    }

                    for (QDockWidget* additionalDock : plugin->getAdditionalDocks()) {
                        additionalDock->setParent(&window);
                        additionalDock->setAttribute(Qt::WA_DeleteOnClose, false);
                        window.addDockWidget(Qt::RightDockWidgetArea, additionalDock);
                        if (mapDock) {
                            window.tabifyDockWidget(additionalDock, mapDock);
                        }
                        additionalDock->hide();
                    }
                }
            }
        }
        }

        if (!window.restoreDockState() && mapDock) {
            window.resizeDocks({mapDock}, {500}, Qt::Horizontal);
        }

        window.getPluginManager()->setupMenu(window.menuBar());
        window.setupLanguageMenu();
        window.setupConnectionLeds();
        window.setupToolbar();

        QAction* pluginMgrAction = window.menuBar()->addAction(MainWindow::tr("Plugin Manager"));
        QObject::connect(pluginMgrAction, &QAction::triggered, &window, &MainWindow::showPluginManager);

        // EMCON / Go Dark toggle on menu bar
        QAction* emconAction = window.menuBar()->addAction(MainWindow::tr("Go Dark"));
        emconAction->setCheckable(true);
        emconAction->setChecked(false);
        emconAction->setToolTip(MainWindow::tr("Emission Control — stop outgoing transmissions"));
        QObject::connect(emconAction, &QAction::triggered, &window, &MainWindow::toggleEmcon);
        window.setEmconToggleAction(emconAction);
    } else {
        qWarning() << "Plugin directory not found:" << pluginPath;
    }

    window.show();

    return app.exec();
}
