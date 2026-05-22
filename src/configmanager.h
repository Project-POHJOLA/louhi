#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <QObject>
#include <QJsonObject>
#include <QString>
#include <QMap>

class ConfigManager : public QObject
{
    Q_OBJECT

public:
    explicit ConfigManager(QObject* parent = nullptr);
    ~ConfigManager();

    bool loadConfig(const QString& filePath = QString());
    bool saveConfig(const QString& filePath = QString());

    QJsonObject getAppConfig() const { return m_appConfig; }
    void setAppConfig(const QJsonObject& config);

    QJsonObject getPluginConfig(const QString& pluginId) const;
    void setPluginConfig(const QString& pluginId, const QJsonObject& config);

    QJsonObject getSharedConfig(const QString& key) const;
    void setSharedConfig(const QString& key, const QJsonObject& config);

    QString configFilePath() const { return m_configFilePath; }

    static QString defaultConfigPath();

signals:
    void configLoaded();
    void configSaved();
    void configChanged();

private:
    QString m_configFilePath;
    QJsonObject m_appConfig;
    QJsonObject m_pluginConfigs;
    QJsonObject m_sharedConfig;
};

#endif