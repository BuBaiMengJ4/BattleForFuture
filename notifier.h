#ifndef NOTIFIER_H
#define NOTIFIER_H

#include <QObject>
#include <QString>

class QSystemTrayIcon;

/**
 * @brief 跨平台的“系统推送”封装
 *
 * 目标：应用启动时把鼓励语句以**系统通知**的形式弹出来，而不是只写在界面里。
 *
 * Qt 本身没有统一的通知 API，这里按平台能力分三层处理：
 *
 * 1. **首选 QSystemTrayIcon::showMessage()** —— Qt 在各大平台都实现了：
 *    - Windows：通过外壳的通知区域（托盘气泡 / 操作中心）；
 *    - macOS：走 NSUserNotification / UserNotifications；
 *    - Linux：优先经 org.freedesktop.Notifications（D-Bus）交给通知守护进程。
 *    这条路径不需要额外依赖，是跨平台兼容性最好的一种。
 *
 * 2. **Linux 无托盘时的兜底**：很多精简桌面（i3/sway/部分 Wayland 会话）没有
 *    托盘区域，但通常装有 notify-send（libnotify），于是退化为调用
 *    `notify-send <标题> <正文>`。
 *
 * 3. 两者都不可用时只记录日志，绝不因为“推送失败”影响程序启动。
 *
 * 托盘图标只在真正需要推送时才创建（惰性），因此关闭推送的用户不会平白
 * 多出一个托盘图标。
 */
class Notifier : public QObject
{
    Q_OBJECT

public:
    static Notifier &instance();

    /**
     * @brief 发送一条系统通知
     * @return true 表示已经交给系统投递；false 表示当前环境不支持（已记录日志）
     */
    bool notify(const QString &title, const QString &body);

    /// 当前环境是否具备推送能力（仅用于日志/提示，不保证 100% 成功）
    bool isAvailable();

signals:
    /// 用户点击了托盘图标（或菜单里的“显示主界面”）
    void showRequested();
    /// 用户选择了“退出”
    void quitRequested();

private:
    explicit Notifier(QObject *parent = nullptr);

    /// 惰性创建托盘图标；返回是否可用
    bool ensureTray();
    /// Linux 无托盘时的兜底：调用 notify-send
    bool notifyViaNotifySend(const QString &title, const QString &body);

    QSystemTrayIcon *m_tray = nullptr;
};

#endif // NOTIFIER_H
