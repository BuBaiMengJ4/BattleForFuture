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
#include <QComboBox>
#include <QCheckBox>
#include <QPropertyAnimation>
#include <QUrl>
#include <QDesktopServices>
#include <QFile>
#include <QLineEdit>
#include <QFontDatabase>

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

    //计算相差天数
    int calculateRemainingDays(const QDate &targetDate);
    //设置导航菜单
    QListWidget* categoryList =new QListWidget;

    // 创建左侧分类列表
    QListWidget* createCategoryList();

    //设置“常规设置”详细内容
    QWidget* createGeneralPage();
    //设置“界面选项”详细内容
    QWidget* createAppearancePage();
    //设置“高级设置”详细内容
    QWidget* createAdvancedPage();
    //设置“关于”详细内容
    QWidget* createAboutPage();
};
#endif // MAINWINDOW_H
