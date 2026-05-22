#include "locationsettingsdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGridLayout>

LocationSettingsDialog::LocationSettingsDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Location Provider Settings"));
    setMinimumSize(600, 500);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    QGroupBox* mainProviderGroup = new QGroupBox(tr("Main Provider"), this);
    QVBoxLayout* mainProviderLayout = new QVBoxLayout(mainProviderGroup);

    QHBoxLayout* mainTypeLayout = new QHBoxLayout();
    mainTypeLayout->addWidget(new QLabel(tr("Type:"), mainProviderGroup));
    m_mainProviderType = new QComboBox(mainProviderGroup);
    m_mainProviderType->addItems({tr("Serial GPS"), tr("GPSD"), tr("Manual")});
    mainTypeLayout->addWidget(m_mainProviderType);
    mainTypeLayout->addStretch();
    mainProviderLayout->addLayout(mainTypeLayout);

    m_mainProviderTabs = new QTabWidget(mainProviderGroup);

    QWidget* serialWidget = new QWidget();
    QFormLayout* serialLayout = new QFormLayout(serialWidget);
    m_serialPortEdit = new QLineEdit("/dev/ttyUSB0", serialWidget);
    m_serialPortEdit->setPlaceholderText(tr("/dev/ttyUSB0 or /dev/ttyACM0"));
    serialLayout->addRow(tr("Serial Port:"), m_serialPortEdit);
    m_serialBaudSpin = new QSpinBox(serialWidget);
    m_serialBaudSpin->setRange(1200, 115200);
    m_serialBaudSpin->setValue(9600);
    m_serialBaudSpin->setSingleStep(1200);
    serialLayout->addRow(tr("Baud Rate:"), m_serialBaudSpin);
    m_mainProviderTabs->addTab(serialWidget, tr("Serial"));

    QWidget* gpsdWidget = new QWidget();
    QFormLayout* gpsdLayout = new QFormLayout(gpsdWidget);
    m_gpsdHostEdit = new QLineEdit("localhost", gpsdWidget);
    gpsdLayout->addRow(tr("Host:"), m_gpsdHostEdit);
    m_gpsdPortSpin = new QSpinBox(gpsdWidget);
    m_gpsdPortSpin->setRange(1, 65535);
    m_gpsdPortSpin->setValue(2947);
    gpsdLayout->addRow(tr("Port:"), m_gpsdPortSpin);
    m_mainProviderTabs->addTab(gpsdWidget, tr("GPSD"));

    QWidget* manualWidget = new QWidget();
    QFormLayout* manualLayout = new QFormLayout(manualWidget);
    m_manualLatSpin = new QDoubleSpinBox(manualWidget);
    m_manualLatSpin->setRange(-90.0, 90.0);
    m_manualLatSpin->setDecimals(6);
    m_manualLatSpin->setValue(0.0);
    manualLayout->addRow(tr("Latitude:"), m_manualLatSpin);
    m_manualLonSpin = new QDoubleSpinBox(manualWidget);
    m_manualLonSpin->setRange(-180.0, 180.0);
    m_manualLonSpin->setDecimals(6);
    m_manualLonSpin->setValue(0.0);
    manualLayout->addRow(tr("Longitude:"), m_manualLonSpin);
    m_manualAltSpin = new QDoubleSpinBox(manualWidget);
    m_manualAltSpin->setRange(-500.0, 9000.0);
    m_manualAltSpin->setDecimals(1);
    m_manualAltSpin->setValue(0.0);
    manualLayout->addRow(tr("Altitude (m):"), m_manualAltSpin);
    m_mainProviderTabs->addTab(manualWidget, tr("Manual"));

    mainProviderLayout->addWidget(m_mainProviderTabs);
    mainLayout->addWidget(mainProviderGroup);

    QGroupBox* fallbackProviderGroup = new QGroupBox(tr("Fallback Provider"), this);
    QVBoxLayout* fallbackProviderLayout = new QVBoxLayout(fallbackProviderGroup);

    QHBoxLayout* fallbackTypeLayout = new QHBoxLayout();
    fallbackTypeLayout->addWidget(new QLabel(tr("Type:"), fallbackProviderGroup));
    m_fallbackProviderType = new QComboBox(fallbackProviderGroup);
    m_fallbackProviderType->addItems({tr("None"), tr("Serial GPS"), tr("GPSD"), tr("Manual")});
    fallbackTypeLayout->addWidget(m_fallbackProviderType);
    fallbackTypeLayout->addStretch();
    fallbackProviderLayout->addLayout(fallbackTypeLayout);

    m_fallbackProviderTabs = new QTabWidget(fallbackProviderGroup);

    QWidget* serialWidgetFallback = new QWidget();
    QFormLayout* serialLayoutFallback = new QFormLayout(serialWidgetFallback);
    m_serialPortEditFallback = new QLineEdit("/dev/ttyUSB1", serialWidgetFallback);
    m_serialPortEditFallback->setPlaceholderText(tr("/dev/ttyUSB1 or /dev/ttyACM0"));
    serialLayoutFallback->addRow(tr("Serial Port:"), m_serialPortEditFallback);
    m_serialBaudSpinFallback = new QSpinBox(serialWidgetFallback);
    m_serialBaudSpinFallback->setRange(1200, 115200);
    m_serialBaudSpinFallback->setValue(9600);
    m_serialBaudSpinFallback->setSingleStep(1200);
    serialLayoutFallback->addRow(tr("Baud Rate:"), m_serialBaudSpinFallback);
    m_fallbackProviderTabs->addTab(serialWidgetFallback, tr("Serial"));

    QWidget* gpsdWidgetFallback = new QWidget();
    QFormLayout* gpsdLayoutFallback = new QFormLayout(gpsdWidgetFallback);
    m_gpsdHostEditFallback = new QLineEdit("localhost", gpsdWidgetFallback);
    gpsdLayoutFallback->addRow(tr("Host:"), m_gpsdHostEditFallback);
    m_gpsdPortSpinFallback = new QSpinBox(gpsdWidgetFallback);
    m_gpsdPortSpinFallback->setRange(1, 65535);
    m_gpsdPortSpinFallback->setValue(2947);
    gpsdLayoutFallback->addRow(tr("Port:"), m_gpsdPortSpinFallback);
    m_fallbackProviderTabs->addTab(gpsdWidgetFallback, tr("GPSD"));

    QWidget* manualWidgetFallback = new QWidget();
    QFormLayout* manualLayoutFallback = new QFormLayout(manualWidgetFallback);
    m_manualLatSpinFallback = new QDoubleSpinBox(manualWidgetFallback);
    m_manualLatSpinFallback->setRange(-90.0, 90.0);
    m_manualLatSpinFallback->setDecimals(6);
    m_manualLatSpinFallback->setValue(0.0);
    manualLayoutFallback->addRow(tr("Latitude:"), m_manualLatSpinFallback);
    m_manualLonSpinFallback = new QDoubleSpinBox(manualWidgetFallback);
    m_manualLonSpinFallback->setRange(-180.0, 180.0);
    m_manualLonSpinFallback->setDecimals(6);
    m_manualLonSpinFallback->setValue(0.0);
    manualLayoutFallback->addRow(tr("Longitude:"), m_manualLonSpinFallback);
    m_manualAltSpinFallback = new QDoubleSpinBox(manualWidgetFallback);
    m_manualAltSpinFallback->setRange(-500.0, 9000.0);
    m_manualAltSpinFallback->setDecimals(1);
    m_manualAltSpinFallback->setValue(0.0);
    manualLayoutFallback->addRow(tr("Altitude (m):"), m_manualAltSpinFallback);
    m_fallbackProviderTabs->addTab(manualWidgetFallback, tr("Manual"));

    fallbackProviderLayout->addWidget(m_fallbackProviderTabs);
    mainLayout->addWidget(fallbackProviderGroup);

    QGroupBox* broadcastGroup = new QGroupBox(tr("Broadcast Settings"), this);
    QFormLayout* broadcastLayout = new QFormLayout(broadcastGroup);

    m_broadcastOnChange = new QCheckBox(tr("Broadcast on location change"), broadcastGroup);
    m_broadcastOnChange->setChecked(true);
    broadcastLayout->addRow(m_broadcastOnChange);

    m_broadcastIntervalSpin = new QSpinBox(broadcastGroup);
    m_broadcastIntervalSpin->setRange(100, 60000);
    m_broadcastIntervalSpin->setValue(1000);
    m_broadcastIntervalSpin->setSingleStep(100);
    broadcastLayout->addRow(tr("Min interval (ms):"), m_broadcastIntervalSpin);

    m_publishTopicEdit = new QLineEdit("location.position", broadcastGroup);
    broadcastLayout->addRow(tr("Publish topic:"), m_publishTopicEdit);

    m_requestTopicEdit = new QLineEdit("location.request", broadcastGroup);
    broadcastLayout->addRow(tr("Request topic:"), m_requestTopicEdit);

    mainLayout->addWidget(broadcastGroup);

    connect(m_mainProviderType, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
                m_mainProviderTabs->setCurrentIndex(index);
            });

    connect(m_fallbackProviderType, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
                if (index > 0) {
                    m_fallbackProviderTabs->setCurrentIndex(index - 1);
                }
            });

    QHBoxLayout* buttonLayout = new QHBoxLayout();
    QPushButton* okBtn = new QPushButton(tr("OK"), this);
    QPushButton* cancelBtn = new QPushButton(tr("Cancel"), this);
    buttonLayout->addStretch();
    buttonLayout->addWidget(okBtn);
    buttonLayout->addWidget(cancelBtn);

    mainLayout->addLayout(buttonLayout);

    connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
}

LocationSettingsDialog::~LocationSettingsDialog()
{
}

LocationProviderConfig LocationSettingsDialog::mainProvider() const
{
    return saveFormToProvider("main");
}

LocationProviderConfig LocationSettingsDialog::fallbackProvider() const
{
    return saveFormToProvider("fallback");
}

bool LocationSettingsDialog::broadcastOnChange() const
{
    return m_broadcastOnChange->isChecked();
}

int LocationSettingsDialog::broadcastInterval() const
{
    return m_broadcastIntervalSpin->value();
}

QString LocationSettingsDialog::publishTopic() const
{
    return m_publishTopicEdit->text();
}

QString LocationSettingsDialog::requestTopic() const
{
    return m_requestTopicEdit->text();
}

void LocationSettingsDialog::setMainProvider(const LocationProviderConfig& config)
{
    loadProviderToForm("main", config);
}

void LocationSettingsDialog::setFallbackProvider(const LocationProviderConfig& config)
{
    loadProviderToForm("fallback", config);
}

void LocationSettingsDialog::setBroadcastOnChange(bool broadcast)
{
    m_broadcastOnChange->setChecked(broadcast);
}

void LocationSettingsDialog::setBroadcastInterval(int interval)
{
    m_broadcastIntervalSpin->setValue(interval);
}

void LocationSettingsDialog::setPublishTopic(const QString& topic)
{
    m_publishTopicEdit->setText(topic);
}

void LocationSettingsDialog::setRequestTopic(const QString& topic)
{
    m_requestTopicEdit->setText(topic);
}

void LocationSettingsDialog::onMainProviderTypeChanged(const QString& type)
{
    Q_UNUSED(type);
}

void LocationSettingsDialog::onFallbackProviderTypeChanged(const QString& type)
{
    Q_UNUSED(type);
}

void LocationSettingsDialog::loadProviderToForm(const QString& prefix, const LocationProviderConfig& config)
{
    if (config.type == "serial") {
        if (prefix == "main") {
            m_mainProviderType->setCurrentIndex(0);
            m_serialPortEdit->setText(config.providerConfig.value("port").toString("/dev/ttyUSB0"));
            m_serialBaudSpin->setValue(config.providerConfig.value("baudRate").toInt(9600));
        } else {
            m_fallbackProviderType->setCurrentIndex(1);
            m_serialPortEditFallback->setText(config.providerConfig.value("port").toString("/dev/ttyUSB1"));
            m_serialBaudSpinFallback->setValue(config.providerConfig.value("baudRate").toInt(9600));
        }
    } else if (config.type == "gpsd") {
        if (prefix == "main") {
            m_mainProviderType->setCurrentIndex(1);
            m_gpsdHostEdit->setText(config.providerConfig.value("host").toString("localhost"));
            m_gpsdPortSpin->setValue(config.providerConfig.value("port").toInt(2947));
        } else {
            m_fallbackProviderType->setCurrentIndex(2);
            m_gpsdHostEditFallback->setText(config.providerConfig.value("host").toString("localhost"));
            m_gpsdPortSpinFallback->setValue(config.providerConfig.value("port").toInt(2947));
        }
    } else if (config.type == "manual") {
        if (prefix == "main") {
            m_mainProviderType->setCurrentIndex(2);
            m_manualLatSpin->setValue(config.providerConfig.value("latitude").toDouble(0.0));
            m_manualLonSpin->setValue(config.providerConfig.value("longitude").toDouble(0.0));
            m_manualAltSpin->setValue(config.providerConfig.value("altitude").toDouble(0.0));
        } else {
            m_fallbackProviderType->setCurrentIndex(3);
            m_manualLatSpinFallback->setValue(config.providerConfig.value("latitude").toDouble(0.0));
            m_manualLonSpinFallback->setValue(config.providerConfig.value("longitude").toDouble(0.0));
            m_manualAltSpinFallback->setValue(config.providerConfig.value("altitude").toDouble(0.0));
        }
    }
}

LocationProviderConfig LocationSettingsDialog::saveFormToProvider(const QString& prefix) const
{
    LocationProviderConfig config;
    config.enabled = true;

    if (prefix == "main") {
        int typeIndex = m_mainProviderType->currentIndex();
        if (typeIndex == 0) {
            config.type = "serial";
            config.name = tr("Serial GPS");
            config.providerConfig["port"] = m_serialPortEdit->text();
            config.providerConfig["baudRate"] = m_serialBaudSpin->value();
        } else if (typeIndex == 1) {
            config.type = "gpsd";
            config.name = tr("GPSD");
            config.providerConfig["host"] = m_gpsdHostEdit->text();
            config.providerConfig["port"] = m_gpsdPortSpin->value();
        } else {
            config.type = "manual";
            config.name = tr("Manual");
            config.providerConfig["latitude"] = m_manualLatSpin->value();
            config.providerConfig["longitude"] = m_manualLonSpin->value();
            config.providerConfig["altitude"] = m_manualAltSpin->value();
        }
    } else {
        int typeIndex = m_fallbackProviderType->currentIndex();
        if (typeIndex == 0) {
            config.type = "none";
            config.name = tr("None");
            config.enabled = false;
        } else if (typeIndex == 1) {
            config.type = "serial";
            config.name = tr("Serial GPS (Fallback)");
            config.providerConfig["port"] = m_serialPortEditFallback->text();
            config.providerConfig["baudRate"] = m_serialBaudSpinFallback->value();
        } else if (typeIndex == 2) {
            config.type = "gpsd";
            config.name = tr("GPSD (Fallback)");
            config.providerConfig["host"] = m_gpsdHostEditFallback->text();
            config.providerConfig["port"] = m_gpsdPortSpinFallback->value();
        } else {
            config.type = "manual";
            config.name = tr("Manual (Fallback)");
            config.providerConfig["latitude"] = m_manualLatSpinFallback->value();
            config.providerConfig["longitude"] = m_manualLonSpinFallback->value();
            config.providerConfig["altitude"] = m_manualAltSpinFallback->value();
        }
    }

    config.id = config.type + "_" + config.name.toLower().replace(" ", "_");
    return config;
}
