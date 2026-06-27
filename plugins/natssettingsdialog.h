#ifndef NATSSETTINGSDIALOG_H
#define NATSSETTINGSDIALOG_H

#include <QDialog>
#include <QListWidget>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>

#include "natsplugin.h"

class NatsSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit NatsSettingsDialog(QWidget* parent = nullptr);
    ~NatsSettingsDialog();

    QList<NatsServerConfig> serverConfigs() const;
    void setServerConfigs(const QList<NatsServerConfig>& configs);

    bool emconEnabled() const;
    void setEmconEnabled(bool active);
    void addServer();
    void removeServer();

private slots:
    void onServerSelected();

private:
    void loadCurrentServerToForm();
    void saveFormToCurrentServer();
    void updateServerListDisplay();

    QList<NatsServerConfig> m_servers;
    int m_currentIndex;

    QListWidget* m_serverList;
    QPushButton* m_addBtn;
    QPushButton* m_removeBtn;

    QLineEdit* m_nameEdit;
    QLineEdit* m_urlEdit;
    QSpinBox* m_portSpin;
    QCheckBox* m_autoConnectCheck;
    QCheckBox* m_emconCheck;
};

#endif
