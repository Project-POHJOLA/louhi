#include "configmanager.h"
#include "croutil.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QDir>
#include <QDebug>

ConfigManager::ConfigManager(QObject* parent)
    : QObject(parent)
{
    m_configFilePath = defaultConfigPath();
}

ConfigManager::~ConfigManager()
{
}

QString ConfigManager::defaultConfigPath()
{
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    return configDir + "/config.json";
}

bool ConfigManager::loadConfig(const QString& filePath)
{
    QString path = filePath.isEmpty() ? m_configFilePath : filePath;

    QFile file(path);
    if (!file.exists()) {
        qDebug() << "Config file does not exist, using defaults:" << path;
        m_appConfig = QJsonObject();
        m_pluginConfigs = QJsonObject();
        return true;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open config file:" << path;
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "Failed to parse config JSON:" << parseError.errorString();
        return false;
    }

    QJsonObject root = doc.object();
    m_appConfig = root.value("app").toObject();
    m_pluginConfigs = root.value("plugins").toObject();
    m_sharedConfig = root.value("shared").toObject();

    for (auto it = m_pluginConfigs.begin(); it != m_pluginConfigs.end(); ++it) {
        QJsonObject pluginCfg = it.value().toObject();
        CryUtil::decryptConfig(pluginCfg);
        it.value() = pluginCfg;
    }

    m_configFilePath = path;
    emit configLoaded();
    return true;
}

bool ConfigManager::saveConfig(const QString& filePath)
{
    QString path = filePath.isEmpty() ? m_configFilePath : filePath;

    QFileInfo fileInfo(path);
    QDir dir = fileInfo.dir();
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    QJsonObject pluginConfigs = m_pluginConfigs;
    for (auto it = pluginConfigs.begin(); it != pluginConfigs.end(); ++it) {
        QJsonObject pluginCfg = it.value().toObject();
        CryUtil::encryptConfig(pluginCfg);
        it.value() = pluginCfg;
    }

    QJsonObject root;
    root["app"] = m_appConfig;
    root["plugins"] = pluginConfigs;
    root["shared"] = m_sharedConfig;

    QJsonDocument doc(root);

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to open config file for writing:" << path;
        return false;
    }

    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    m_configFilePath = path;
    emit configSaved();
    return true;
}

void ConfigManager::setAppConfig(const QJsonObject& config)
{
    m_appConfig = config;
    emit configChanged();
}

QJsonObject ConfigManager::getPluginConfig(const QString& pluginId) const
{
    return m_pluginConfigs.value(pluginId).toObject();
}

void ConfigManager::setPluginConfig(const QString& pluginId, const QJsonObject& config)
{
    m_pluginConfigs[pluginId] = config;
    emit configChanged();
}

QJsonObject ConfigManager::getSharedConfig(const QString& key) const
{
    return m_sharedConfig.value(key).toObject();
}

void ConfigManager::setSharedConfig(const QString& key, const QJsonObject& config)
{
    m_sharedConfig[key] = config;
    emit configChanged();
}