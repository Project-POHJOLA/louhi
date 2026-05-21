#ifndef MESSAGEVIEWERPLUGIN_H
#define MESSAGEVIEWERPLUGIN_H

#include "../src/plugininterface.h"
#include <QString>
#include <QStringList>
#include <QTextEdit>
#include <QListWidget>
#include <QJsonObject>
#include <QtPlugin>

class MessageViewerPlugin : public PluginInterface
{
    Q_OBJECT
    Q_INTERFACES(PluginInterface)

public:
    Q_PLUGIN_METADATA(IID "com.louhi.plugininterface/1.0" FILE "messageviewerplugin.json")
    MessageViewerPlugin(QObject* parent = nullptr);
    ~MessageViewerPlugin();

    PluginInfo getPluginInfo() const override;
    QVector<MenuEntry> getMenuEntries() const override;

    bool load() override;
    bool initialize() override;
    bool start() override;
    bool stop() override;
    bool unload() override;

    QWidget* getWidget() override;
    void configure(QWidget* parent) override;

    QJsonObject getConfig() const override;
    void setConfig(const QJsonObject& config) override;

    void deliverMessage(const QString& topic, const QString& payload) override;

private slots:
    void clearMessages();
    void addTopicSubscription();
    void removeTopicSubscription();

private:
    PluginInfo m_info;
    QWidget* m_widget;
    QListWidget* m_messageList;
    QTextEdit* m_detailView;
    QListWidget* m_topicList;
    int m_maxMessages;
};

#endif