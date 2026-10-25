#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QDate>
#include <QMainWindow>
#include <QString>
#include <QStringList>

class Ui_MainWindow;
class QCloseEvent;
class QTimer;
class SettingsDialog;
class WidgetConfig;

/**
 * @brief 主窗口：高考倒计时 + 设置入口 + 桌面小组件宿主
 *
 * 职责划分：
 * - 本类只负责“首页显示 / 小组件生命周期 / 配置文件路径”；
 * - 全部设置界面逻辑搬到 SettingsDialog，两边通过信号通信，
 *   避免一个文件里堆叠上千行 UI 代码（旧实现 mainwindow.cpp 近千行）；
 * - 小组件按配置的样式创建：样式 1 = 圆角卡片，样式 2 = 右下角透明区域，
 *   两者都支持自定义文字颜色与 LCD 数字颜色。
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

public slots:
    /// 进入桌面小组件模式（也会被“小部件模式”按钮触发）
    void enterWidgetMode();
    /// 关闭小组件、回到主窗口，并把窗口带到前台
    void returnToMainWindow();
    /// 把当前可见的窗口带到前台（被其它实例请求显示时调用）
    void bringToFront();

private slots:
    void on_Settings_clicked();
    void on_pushButton_clicked();
    /// 刷新主页倒计时，并按需同步正在运行的小组件数值
    void refreshCountdowns();

protected:
    /// 关闭主界面时连同小组件一起退出，避免留下无可见窗口的僵尸实例
    void closeEvent(QCloseEvent *event) override;

private:
    // ---------- 小组件 ----------
    void createDesktopWidget();
    void destroyDesktopWidget();
    bool widgetModeActive() const;

    // ---------- 配置与数据 ----------
    /// 计算相差天数（目标日期已过返回 0）
    static int calculateRemainingDays(const QDate &targetDate);
    /// 读取配置中的高考年份（默认当前年，非法值回退默认）
    int readAcademyYear() const;
    /// 根据高考年份刷新高考/一模日期
    void refreshTargetDates();
    /// 加载鼓励语句文件；成功返回 true
    bool loadQuotes();
    /// 随机取一条鼓励语（空安全）
    QString randomQuote() const;
    /// 读取“启动时推送鼓励语句”开关（兼容 true/1/yes/on 等写法）
    bool readNoticeEnabled() const;
    /**
     * @brief 刷新首页底部的鼓励语句
     *
     * 只在“当前语句为空”时才随机抽取，保证同一条语句既用于推送、也用于首页显示，
     * 不会因为重复调用而变成两句不同的话。
     */
    void refreshQuoteLabel();
    /**
     * @brief 通过系统通知推送当前鼓励语句（应用启动时调用）
     *
     * 推送内容与首页显示的是**同一条**语句；具体投递方式由 Notifier 决定
     * （托盘气泡优先，Linux 无托盘时退化为 notify-send）。
     */
    void pushStartupNotification();
    /// 为首页设置一个自带渐变背景的调色板
    void applyHomeBackground();

    // ---------- 成员 ----------
    Ui_MainWindow *ui;

    QString appDir;             ///< 应用程序所在目录
    QString configPath;         ///< config.ini 完整路径
    QString quotesFilePath;     ///< 鼓励语句文件完整路径
    QStringList quotes;         ///< 鼓励语句列表

    QDate gaokaoDate;           ///< 高考日期（依据配置的年份动态计算）
    QDate yimoDate;             ///< 一模日期

    SettingsDialog *m_settings = nullptr;   ///< 惰性创建，整个生命周期内复用一个实例
    QWidget        *m_desktopWidget = nullptr; ///< 小组件实例（样式 1 或 2），防止泄漏
    QTimer         *m_countdownTimer = nullptr; ///< 跨零点自动刷新倒计时
    QString         m_pushedQuote;      ///< 当前推送的语句（首页与会话内保持同一条）
};

#endif // MAINWINDOW_H
