#ifndef UKUIGTKCONFIG_H
#define UKUIGTKCONFIG_H

#include <QObject>
#include "QGSettings/qgsettings.h"
class UkuiGtkConfig : public QObject
{
    Q_OBJECT
public:
    UkuiGtkConfig();
    ~UkuiGtkConfig();

    void connectGsettingSignal();
    void addImportStatementsToGtkCssUserFile();
    void modifyColorsCssFile(QString colorsDefinitions);
    QString converRGBToHex(QColor color);
public Q_SLOTS:
    void doGsettingsChanged(QString key);
private:
     QGSettings *m_colorGsettings;
     QGSettings *m_gtkThemeGsettings;
};

#endif // UKUIGTKCONFIG_H
