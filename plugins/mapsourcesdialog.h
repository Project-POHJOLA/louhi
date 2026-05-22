#ifndef MAPSOURCESDIALOG_H
#define MAPSOURCESDIALOG_H

#include <QDialog>
#include <QList>
#include <QVariantMap>
#include "mapsources.h"

class QListWidget;
class QLineEdit;
class QComboBox;
class QStackedWidget;
class QPushButton;

class MapSourcesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MapSourcesDialog(const QList<MapSource>& customSources,
                              QWidget* parent = nullptr);

    QList<MapSource> customSources() const;

private slots:
    void onAddXyz();
    void onAddWms();
    void onRemove();

private:
    void buildUi();
    void refreshList();
    bool validateXyzInput(const QString& url) const;

    QList<MapSource> m_customSources;

    QListWidget* m_sourceList;
    QPushButton* m_addXyzBtn;
    QPushButton* m_addWmsBtn;
    QPushButton* m_removeBtn;
};

#endif
