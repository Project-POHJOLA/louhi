#ifndef PLUGININTERFACE_H
#define PLUGININTERFACE_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QMenuBar>
#include <QMap>
#include <QJsonObject>

enum class PluginType {
    Communication,
    Map,
    Screen
};

struct PluginInfo {
    QString id;
    QString name;
    QString version;
    QString description;
    QString author;
    PluginType type;
    bool enabled;
    QStringList dependencies;
    QStringList capabilities;
    QStringList subscribeTopics;
    QStringList publishTopics;
};

struct MenuEntry {
    QString topMenu;
    QStringList subMenus;
};

class PluginInterface : public QObject
{
    Q_OBJECT

public:
    explicit PluginInterface(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~PluginInterface() = default;

    virtual PluginInfo getPluginInfo() const = 0;
    virtual QVector<MenuEntry> getMenuEntries() const = 0;

    virtual bool load() = 0;
    virtual bool initialize() = 0;
    virtual bool start() = 0;
    virtual bool stop() = 0;
    virtual bool unload() = 0;

    virtual QWidget* getWidget() = 0;
    virtual void configure(QWidget* parent) = 0;

    virtual QJsonObject getConfig() const = 0;
    virtual void setConfig(const QJsonObject& config) = 0;

    virtual void setSubscribedTopics(const QStringList& topics) { Q_UNUSED(topics); }

public slots:
    virtual void deliverMessage(const QString& topic, const QString& payload) {
        emit messageReceived(topic, payload);
    }

signals:
    void messageReceived(const QString& topic, const QString& payload);
    void statusChanged(const QString& status);
    void showWidgetRequested();
    void connectionStatusChanged(const QString& connectionName, const QString& status);
};

#define PluginInterface_iid "com.louhi.plugininterface/1.0"
Q_DECLARE_INTERFACE(PluginInterface, PluginInterface_iid)

#endif