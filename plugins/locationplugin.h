#ifndef LOCATIONPLUGIN_H
#define LOCATIONPLUGIN_H

#include "../src/plugininterface.h"
#include "gpsprovider.h"
#include "locationsettingsdialog.h"
#include <QJsonObject>
#include <QTimer>
#include <QtPlugin>

class QWidget;
class QVBoxLayout;
class QLabel;

class LocationPlugin : public PluginInterface
{
    Q_OBJECT
    Q_INTERFACES(PluginInterface)
    Q_PLUGIN_METADATA(IID "com.louhi.plugininterface/1.0" FILE "locationplugin.json")

public:
    LocationPlugin(QObject* parent = nullptr);
    ~LocationPlugin();

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

    void setSubscribedTopics(const QStringList& topics) override;

private slots:
    void onLocationUpdated(const LocationData& location);
    void onMainProviderError(const QString& error);
    void onFallbackProviderError(const QString& error);
    void onLocationRequest(const QString& topic, const QString& payload);
    void onBroadcastTimer();

private:
    void buildStatusWidget();
    void updateStatusDisplay();
    void switchToFallback();
    void switchToMain();
    void broadcastLocation(const LocationData& location, bool isReply = false);
    GpsProvider* createProvider(const LocationProviderConfig& config);

    PluginInfo m_info;
    QWidget* m_statusWidget;
    QVBoxLayout* m_mainLayout;
    QLabel* m_statusLabel;
    QLabel* m_locationLabel;
    QLabel* m_providerLabel;

    GpsProvider* m_mainProvider;
    GpsProvider* m_fallbackProvider;
    GpsProvider* m_activeProvider;

    LocationProviderConfig m_mainConfig;
    LocationProviderConfig m_fallbackConfig;

    bool m_broadcastOnChange;
    int m_broadcastInterval;
    QString m_publishTopic;
    QString m_requestTopic;

    QTimer* m_broadcastTimer;
    LocationData m_lastBroadcastLocation;
    qint64 m_lastBroadcastTime;

    bool m_useFallback;
    QStringList m_subscribedTopics;
};

#endif
