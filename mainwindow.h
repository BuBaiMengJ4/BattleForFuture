#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QDate>
#include <QDialog>
#include <QScreen>
#include <QStringList>
#include <QRandomGenerator>
#include <QPushButton>
#include <QDialog>
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

namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);

    //鼓励语句(./text/quotes.txt)
    QStringList quotes;
    // 应用程序所在目录
    QString appDir = QCoreApplication::applicationDirPath();
    // quotes.txt的完整路径
    QString filePath = QDir(appDir).filePath("Assets/quotes.txt");

    ~MainWindow();

private slots:
    void on_Settings_clicked();
    void on_pushButton_clicked();

private:
    Ui::MainWindow *ui;
    //设置对话框
    QDialog *SettingWindow = new QDialog(this);
    //设置右侧堆叠窗口
    QStackedWidget* pagesWidget = nullptr;
    //设置复选框“开机自启动”状态标志，防止被递归调用
    bool m_isSettingAutoStart = false;

    //计算相差天数
    int calculateRemainingDays(const QDate &targetDate);
    //设置导航菜单
    QListWidget* categoryList =new QListWidget;

    // 创建左侧分类列表
    QListWidget* createCategoryList();

    //设置“常规设置”详细内容
    QWidget* createGeneralPage();
    //设置“界面选项”详细内容
    QWidget* createWordEditPage();
    //设置“关于”详细内容
    QWidget* createAboutPage();
};
#endif // MAINWINDOW_H
