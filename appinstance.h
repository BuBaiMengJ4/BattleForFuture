#ifndef APPINSTANCE_H
#define APPINSTANCE_H

#include <QObject>
#include <QString>

class QLocalServer;
class QLocalSocket;

/**
 * @brief 单实例协调器（修复“编译成功但看不到窗口”的关键一环）
 *
 * 旧实现用 QLockFile 做单实例保护，第二个实例会**静默退出**：
 * 用户双击图标后什么都没有发生，看起来就像“程序编译成功但不弹窗口”。
 * 更糟的是开机自启先拉起一个隐藏的小组件实例时，之后手动启动的每一次
 * 都会静默失败，主窗口永远出不来。
 *
 * 现在的做法：
 * - 第一个实例用 QLocalServer 占住一个具名套接字（Windows 下即命名管道）；
 * - 之后的实例连接该套接字并发送 "show" 后正常退出；
 * - 第一个实例收到请求后把窗口带到前台（raise + activateWindow）。
 *
 * 容器 / 受限沙箱里命名管道可能不可用，此时 listen() 会失败：
 * 这种情况只记一条警告日志，程序照常运行（不做单实例限制），绝不阻断启动。
 */
class AppInstance : public QObject
{
    Q_OBJECT

public:
    explicit AppInstance(const QString &key, QObject *parent = nullptr);
    ~AppInstance() override;

    /**
     * @brief 尝试成为主实例
     * @return true 表示当前进程是唯一实例，可以继续启动；
     *         false 表示已有实例在运行（已通知对方显示窗口，本次应退出）。
     */
    bool tryBecomePrimary();

signals:
    /// 另一个实例请求把已有窗口显示到前台
    void showRequested();

private:
    QString        m_key;
    QLocalServer  *m_server = nullptr;
    bool           m_primary = false;
    bool           m_disabled = false;   ///< 平台不支持具名套接字时置位
};

#endif // APPINSTANCE_H
