#ifndef NATSSETTINGSDIALOG_H
#define NATSSETTINGSDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>

class NatsSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit NatsSettingsDialog(QWidget* parent = nullptr);
    ~NatsSettingsDialog();

    QString serverUrl() const;
    int port() const;
    bool autoConnect() const;

    void setServerUrl(const QString& url);
    void setPort(int port);
    void setAutoConnect(bool autoConnect);

private:
    QLineEdit* m_serverUrlEdit;
    QSpinBox* m_portSpin;
    QCheckBox* m_autoConnectCheck;
};

#endif
