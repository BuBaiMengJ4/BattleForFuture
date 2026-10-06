#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QDate>

class Ui_MainWindow;
class QDialog;
class QStackedWidget;
class QListWidget;
class QPushButton;
class QWidget;
class DesktopWidget;

/**
 * @brief 主窗口：高考倒计时 + 设置对话框 + 桌面小组件入口
 *
 * 重构要点：
 * - 头文件不再大量包含 UI 相关头文件（编译依赖最小化，增量编译更快）；
 * - 日期、配置文件路径等公共数据集中管理，消除魔法字符串重复；
 * - 所有文件读写 / 配置解析均带异常与错误处理，并写入 Logs 日志。
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void on_Settings_clicked();       // 打开设置对话框
    void on_pushButton_clicked();     // 缩小为桌面小部件

private:
    Ui_MainWindow *ui;

    // ---------- 公共数据 ----------
    QString appDir;                   // 应用程序所在目录
    QString configPath;               // config.ini 完整路径
    QString quotesFilePath;           // 鼓励语句文件完整路径
    QStringList quotes;               // 鼓励语句列表

    QDate gaokaoDate;                 // 高考日期（依据配置的年份动态计算）
    QDate yimoDate;                   // 一模日期

    // ---------- 设置对话框 ----------
    QDialog *settingDialog = nullptr;         // 惰性创建，避免每次打开重建控件树
    QStackedWidget *pagesWidget = nullptr;
    QListWidget *categoryList = nullptr;
    bool m_isSettingAutoStart = false;        // 防止复选框信号递归

    DesktopWidget *m_desktopWidget = nullptr; // 小组件单例，防止反复创建泄漏

    // ---------- 工具函数 ----------
    /// 计算相差天数（目标日期已过返回 0）
    static int calculateRemainingDays(const QDate &targetDate);
    /// 读取配置中的高考年份（默认当前年），非法值回退默认
    int readAcademyYear() const;
    /// 根据高考年份刷新高考/一模日期
    void refreshTargetDates();
    /// 加载鼓励语句文件；成功返回 true
    bool loadQuotes();
    /// 随机取一条鼓励语（空安全）
    QString randomQuote() const;
    /// 更新主页倒计时显示
    void updateCountdowns();

    // ---------- 设置页面 ----------
    void buildSettingDialog();        // 一次性构建对话框内容
    QListWidget *createCategoryList();
    QWidget *createGeneralPage();
    QWidget *createAppearancePage();
    QWidget *createWordEditPage();
    QWidget *createAboutPage();
    void updateButtonColor(QPushButton *button, const QColor &color);
};

#endif // MAINWINDOW_H
