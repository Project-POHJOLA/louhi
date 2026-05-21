#ifndef TAKSETTINGSDIALOG_H
#define TAKSETTINGSDIALOG_H

#include <QDialog>
#include <QListWidget>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QGroupBox>
#include "takserverconnection.h"

class TakSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TakSettingsDialog(QWidget* parent = nullptr);
    ~TakSettingsDialog();

    QList<TakServerConfig> serverConfigs() const;
    void setServerConfigs(const QList<TakServerConfig>& configs);

private slots:
    void onServerSelected();
    void addServer();
    void removeServer();
    void browseCertFile();

private:
    void loadCurrentServerToForm();
    void saveFormToCurrentServer();
    void updateServerListDisplay();

    QList<TakServerConfig> m_servers;
    int m_currentIndex;

    QListWidget* m_serverList;
    QPushButton* m_addBtn;
    QPushButton* m_removeBtn;

    QLineEdit* m_nameEdit;
    QLineEdit* m_addressEdit;
    QSpinBox* m_portSpin;

    QLineEdit* m_certPathEdit;
    QPushButton* m_browseCertBtn;
    QLineEdit* m_certPasswordEdit;

    QLineEdit* m_callsignEdit;
    QLineEdit* m_cotTypeEdit;
    QComboBox* m_colorCombo;
    QComboBox* m_roleCombo;

    QCheckBox* m_autoConnectCheck;
    QCheckBox* m_debugLoggingCheck;
};

#endif
