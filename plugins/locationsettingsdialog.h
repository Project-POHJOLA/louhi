#ifndef LOCATIONSETTINGSDIALOG_H
#define LOCATIONSETTINGSDIALOG_H

#include <QDialog>
#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QGroupBox>
#include <QPushButton>
#include <QLabel>
#include <QTabWidget>
#include <QJsonObject>

struct LocationProviderConfig {
    QString id;
    QString type;
    QString name;
    bool enabled;

    QJsonObject providerConfig;
};

class LocationSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LocationSettingsDialog(QWidget* parent = nullptr);
    ~LocationSettingsDialog();

    LocationProviderConfig mainProvider() const;
    LocationProviderConfig fallbackProvider() const;
    bool broadcastOnChange() const;
    int broadcastInterval() const;
    QString publishTopic() const;
    QString requestTopic() const;

    void setMainProvider(const LocationProviderConfig& config);
    void setFallbackProvider(const LocationProviderConfig& config);
    void setBroadcastOnChange(bool broadcast);
    void setBroadcastInterval(int interval);
    void setPublishTopic(const QString& topic);
    void setRequestTopic(const QString& topic);

private slots:
    void onMainProviderTypeChanged(const QString& type);
    void onFallbackProviderTypeChanged(const QString& type);

private:
    QWidget* createProviderConfigWidget(const QString& prefix, bool isMain);
    void loadProviderToForm(const QString& prefix, const LocationProviderConfig& config);
    LocationProviderConfig saveFormToProvider(const QString& prefix) const;

    QComboBox* m_mainProviderType;
    QComboBox* m_fallbackProviderType;

    QTabWidget* m_mainProviderTabs;
    QTabWidget* m_fallbackProviderTabs;

    QLineEdit* m_serialPortEdit;
    QSpinBox* m_serialBaudSpin;

    QLineEdit* m_gpsdHostEdit;
    QSpinBox* m_gpsdPortSpin;

    QDoubleSpinBox* m_manualLatSpin;
    QDoubleSpinBox* m_manualLonSpin;
    QDoubleSpinBox* m_manualAltSpin;

    QLineEdit* m_serialPortEditFallback;
    QSpinBox* m_serialBaudSpinFallback;

    QLineEdit* m_gpsdHostEditFallback;
    QSpinBox* m_gpsdPortSpinFallback;

    QDoubleSpinBox* m_manualLatSpinFallback;
    QDoubleSpinBox* m_manualLonSpinFallback;
    QDoubleSpinBox* m_manualAltSpinFallback;

    QCheckBox* m_broadcastOnChange;
    QSpinBox* m_broadcastIntervalSpin;
    QLineEdit* m_publishTopicEdit;
    QLineEdit* m_requestTopicEdit;
};

#endif
