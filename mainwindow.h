#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QDate>
#include <QDialog>
#include <QScreen>
#include <QStringList>
#include <QRandomGenerator>
#include <QPushButton>
#include <QLabel>
#include <QLCDNumber>
#include <QGraphicsDropShadowEffect>
#include <QListWidget>
#include <QStackedWidget>
#include <QGroupBox>
#include <QCheckBox>
#include <QPropertyAnimation>
#include <QUrl>
#include <QDesktopServices>
#include <QFile>
#include <QMessageBox>
#include <QTextEdit>
#include <QDir>
#include <QSettings>
#include <QColorDialog>
#include <QComboBox>
#include <QSystemTrayIcon>
#include <QSysInfo>
#include <QProcess>


namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow: public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget * parent = nullptr);

    //鼓励语句(./Assets/quotes.txt)
    QStringList quotes;
    // 应用程序所在目录
    QString appDir = QCoreApplication::applicationDirPath();
    // quotes.txt的完整路径
    QString filePath = QDir(appDir).filePath("Assets/quotes.txt");

    //返回参加高考年份
    int year()
    {   QSettings* settings = new QSettings(QCoreApplication::applicationDirPath() + "//config.ini", QSettings::IniFormat);
        settings->beginGroup("Global");
        int year = settings->value("academicyear").toInt();
        settings->endGroup();
        delete settings;
        if(year == 0)  //如果配置文件无此值，如果没过今年高考时间，就返回今年，否则返回明年。
        {
            year =QDate::currentDate().year();
            QDate currentDate = QDate::currentDate();
            int date_day = currentDate.daysTo(QDate(year,6,7));
            return date_day>0 ? year : year+1;
        }
        else return year;
    };
    int academicyear = year();

    ~MainWindow();

private slots:
    void on_Settings_clicked();
    void on_pushButton_clicked();

private:
    Ui::MainWindow * ui;

    QDialog * SettingWindow = new QDialog(this);//设置对话框
    QDialog * WidgetWindow = new QDialog(this);//桌面小组件

    QStackedWidget * pagesWidget = nullptr;//设置右侧堆叠窗口
    QListWidget * createCategoryList();// 创建左侧分类列表
    QListWidget * categoryList = new QListWidget;//设置导航菜单

    bool m_isSettingAutoStart = false;//设置复选框“开机自启动”状态标志，防止被递归调用

    int calculateRemainingDays(const QDate & targetDate);//计算相差天数


    void updateButtonColor(QPushButton * button,const QColor & color);//负责按钮样式的更新

    //设置“常规设置”详细内容
    QWidget * createGeneralPage();
    //设置“外观设置”选项
    QWidget * createAppearancePage();
    //设置“文字编辑”详细内容
    QWidget * createWordEditPage();
    //设置“关于”详细内容
    QWidget * createAboutPage();
    //系统托盘
    QSystemTrayIcon *trayIcon;
};
#endif // MAINWINDOW_H

