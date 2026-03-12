#include "color-gtkconfig.h"
#include <QPalette>

#include <QString>
#include <QVariant>
#include <QDebug>
#include <QStandardPaths>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QCoreApplication>
#include <QApplication>
#include "clib-syslog.h"

#define THEME_COLOR        "theme-color"
#define GTK_THEME          "gtk-theme"

UkuiGtkConfig::UkuiGtkConfig()
{
    const QByteArray id(UKUI_STYLE_SCHEMA);
    const QByteArray id2(MATE_INTERFACE_SCHEMA);
    m_colorGsettings = new QGSettings(id);
    m_gtkThemeGsettings = new QGSettings(id2);
}

UkuiGtkConfig::~UkuiGtkConfig(){
    delete m_colorGsettings;
    delete m_gtkThemeGsettings;
};

void UkuiGtkConfig::connectGsettingSignal(){
    QObject::connect(m_colorGsettings, SIGNAL(changed(QString))/*&QGSettings::changed*/, this, SLOT(doGsettingsChanged(QString)));
}

void UkuiGtkConfig::addImportStatementsToGtkCssUserFile(){
    QString gtkStylePath = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/gtk-3.0");

    QDir gtkStyle(gtkStylePath);
    if (!gtkStyle.exists(gtkStylePath)) {
        gtkStyle.mkdir(gtkStylePath);
    }

    QString gtkCssPath = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/gtk-3.0/gtk.css");
    QFile gtkCss(gtkCssPath);

    if (gtkCss.open(QIODevice::ReadWrite)) {
        QByteArray gtkCssContents = gtkCss.readAll().trimmed();

        static const QVector<QByteArray> importStatements{
            QByteArrayLiteral("\n@import 'colors.css';"),
        };

        for (const auto &statement : importStatements) {
            if (!gtkCssContents.contains(statement.trimmed())) {
                gtkCssContents.append(statement);
            }
        }

        gtkCss.remove();
        gtkCss.open(QIODevice::WriteOnly | QIODevice::Text);
        gtkCss.write(gtkCssContents);
    }
}

void UkuiGtkConfig::modifyColorsCssFile(QString colorsDefinitions){
    QString colorsCssPath = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/gtk-3.0/colors.css");
    QFile colorsCss(colorsCssPath);

    if (colorsCss.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        QTextStream colorsCssStream(&colorsCss);
        colorsCssStream << endl << QStringLiteral("@define-color hover_bg_color_ukui ")+colorsDefinitions+QStringLiteral(";");
        colorsCssStream << endl << QStringLiteral("@define-color hover_bd_color_ukui ")+colorsDefinitions+QStringLiteral(";");
        colorsCssStream << endl << QStringLiteral("@define-color active_bg_color_ukui ")+colorsDefinitions+QStringLiteral(";");
        colorsCssStream << endl << QStringLiteral("@define-color active_bd_color_ukui ")+colorsDefinitions+QStringLiteral(";");
        colorsCssStream << endl << QStringLiteral("@define-color selected_bg_color_ukui ")+colorsDefinitions+QStringLiteral(";");
        colorsCssStream << endl << QStringLiteral("@define-color selected_borders_color_ukui ")+colorsDefinitions+QStringLiteral(";");
        colorsCssStream << endl << QStringLiteral("@define-color bt_hover_bg_color_ukui ")+colorsDefinitions+QStringLiteral(";");
        colorsCssStream << endl << QStringLiteral("@define-color bt_hover_bd_color_ukui ")+colorsDefinitions+QStringLiteral(";");
        colorsCssStream << endl << QStringLiteral("@define-color bt_active_bg_color_ukui ")+colorsDefinitions+QStringLiteral(";");
        colorsCssStream << endl << QStringLiteral("@define-color bt_active_bd_color_ukui ")+colorsDefinitions+QStringLiteral(";");
        colorsCssStream << endl << QStringLiteral("@define-color bt_selected_bg_color_ukui ")+colorsDefinitions+QStringLiteral(";");
        colorsCssStream << endl << QStringLiteral("@define-color bt_selected_borders_color_ukui ")+colorsDefinitions+QStringLiteral(";");
        colorsCssStream << endl << QStringLiteral("@define-color crb_active_bg_color_ukui ")+colorsDefinitions+QStringLiteral(";");
        colorsCssStream << endl << QStringLiteral("@define-color crb_hover_bd_color_ukui ")+colorsDefinitions+QStringLiteral(";");
        colorsCssStream << endl << QStringLiteral("@define-color crb_active_bd_color_ukui ")+colorsDefinitions+QStringLiteral(";");
        colorsCssStream << endl << QStringLiteral("@define-color success_color_ukui ")+colorsDefinitions+QStringLiteral(";");
        colorsCssStream << endl << QStringLiteral("@define-color base_active_bg_color_ukui ")+colorsDefinitions+QStringLiteral(";");
        colorsCssStream << endl << QStringLiteral("@define-color base_prelight_bg_color_ukui ")+colorsDefinitions+QStringLiteral(";");
        colorsCssStream << endl << QStringLiteral("@define-color base_selected_bg_color_ukui ")+colorsDefinitions+QStringLiteral(";");
        colorsCssStream << endl << QStringLiteral("@define-color base_checked_bg_color_ukui ")+colorsDefinitions+QStringLiteral(";");
        colorsCssStream << endl << QStringLiteral("@define-color base_active_border_color_ukui ")+colorsDefinitions+QStringLiteral(";");
        colorsCssStream << endl << QStringLiteral("@define-color base_prelight_border_color_ukui ")+colorsDefinitions+QStringLiteral(";");
        colorsCssStream << endl << QStringLiteral("@define-color base_active_bg_image_ukui ")+colorsDefinitions+QStringLiteral(";");
        colorsCssStream << endl << QStringLiteral("@define-color base_checked_bg_image_ukui ")+colorsDefinitions+QStringLiteral(";");
        colorsCssStream << endl << QStringLiteral("@define-color trough_filled_space_normal_color_ukui ")+colorsDefinitions+QStringLiteral(";");
        colorsCssStream << endl << QStringLiteral("@define-color toolbar_button_bg_active_color_ukui ")+colorsDefinitions+QStringLiteral(";");
    }
}


QString UkuiGtkConfig::converRGBToHex(QColor color){
    QString RGBred = QString("%1").arg(color.red(),2,16,QChar('0'));
    QString RGBgreen = QString("%1").arg(color.green(),2,16,QChar('0'));
    QString RGBblue = QString("%1").arg(color.blue(),2,16,QChar('0'));

    QString RGBToHex = "#"+RGBred+RGBgreen+RGBblue;

    return RGBToHex;
}

void UkuiGtkConfig::doGsettingsChanged(QString key)
{
    USD_LOG(LOG_DEBUG,".%s.",key.toLatin1().data());
    if(key == THEME_COLOR){
        USD_LOG(LOG_DEBUG,"..");
        qDebug() << endl << m_colorGsettings->get(THEME_COLOR).toString();
        qDebug() << endl << m_gtkThemeGsettings->get(GTK_THEME).toString();
        QString gtkTheme = m_gtkThemeGsettings->get(GTK_THEME).toString();
        QString themeColor = m_colorGsettings->get(THEME_COLOR).toString();

        if(gtkTheme == "ukui-white" || gtkTheme == "ukui-black"){

            addImportStatementsToGtkCssUserFile();
            QColor colorRGB;

            if (themeColor == "jamPurple") {
                colorRGB = QColor(120, 115, 245);
                qDebug() << endl << colorRGB << endl << themeColor;
            } else if (themeColor == "magenta") {
                colorRGB = QColor(235, 48, 150);
                qDebug() << endl << colorRGB << endl << themeColor;
            } else if (themeColor == "sunRed") {
                colorRGB = QColor(243, 34, 45);
                qDebug() << endl << colorRGB << endl << themeColor;
            } else if (themeColor == "sunsetOrange") {
                colorRGB = QColor(246, 140, 39);
                qDebug() << endl << colorRGB << endl << themeColor;
            } else if (themeColor == "dustGold") {
                colorRGB = QColor(249, 197, 61);
                qDebug() << endl << colorRGB << endl << themeColor;
            } else if (themeColor == "polarGreen") {
                colorRGB = QColor(82, 196, 41);
                qDebug() << endl << colorRGB << endl << themeColor;
            } else {
                colorRGB = QColor(55, 144, 250);
                qDebug() << endl << colorRGB << endl << themeColor;
            }

            QString colorHex = converRGBToHex(colorRGB);
            qDebug() << endl << colorRGB << endl << colorHex;
            modifyColorsCssFile(colorHex);

        } else {
            qDebug() << gtkTheme;
        }
    }
}
