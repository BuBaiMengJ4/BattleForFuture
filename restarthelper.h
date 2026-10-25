#ifndef RESTARTHELPER_H
#define RESTARTHELPER_H

#include <QString>
#include <QStringList>

/**
 * @brief 重启辅助：先退出自己，再由一个“监工”进程等旧进程真正结束后拉起新实例
 *
 * 为什么不能简单地“启动新进程后自己退出”？
 * 本程序有单实例保护（AppInstance 占用一个具名套接字）。重启时旧进程仍然持有
 * 该通道，此刻拉起的新进程一连接就会判定“已有实例在运行”，于是发一条 show
 * 请求就**自行退出**了；而旧进程看到 QProcess::startDetached 返回 true，
 * 以为“新进程已启动”，紧接着自己也退出 —— 结果是程序关掉了却没有重启。
 * 实测就是这个问题。
 *
 * 正确做法：
 * 1. 旧进程用 `--restart-wait <pid>` 启动一个自身副本作为监工；
 * 2. 旧进程立即退出，操作系统随之释放单实例通道；
 * 3. 监工轮询等待 <pid> 结束（或超时兜底），确认通道已释放后再以正常参数
 *    启动新的 GUI 实例。
 *
 * 这样无论单实例通道需要多久才释放，都一定能等到，不依赖任何脆弱的延时猜测。
 */
namespace RestartHelper {

/// 监工模式的命令行开关
extern const char *const kWaitAndRestartFlag;

/// 参数是否是监工模式（形如 --restart-wait <pid>）
bool isSupervisorInvocation(const QStringList &arguments);

/// 监工模式的入口：等待旧进程结束，然后启动新的 GUI 实例
int runSupervisor(const QStringList &arguments);

/**
 * @brief 执行重启：拉起监工进程，然后让调用方退出
 * @param arguments 需要转发给新实例的参数（通常不含监工开关）
 * @return true 表示监工进程已成功创建，调用方应尽快退出
 */
bool launchRestart(const QStringList &arguments);

} // namespace RestartHelper

#endif // RESTARTHELPER_H
