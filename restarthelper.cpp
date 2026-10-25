#include "restarthelper.h"

#include "logger.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QProcess>
#include <QThread>

#ifdef Q_OS_WIN
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#endif

namespace {

/// 等待旧进程退出的最长时间（超时后仍然继续启动，避免卡死）
constexpr int kWaitTimeoutMs = 15000;
/// 轮询间隔
constexpr int kPollIntervalMs = 100;
/// 旧进程退出后，再额外等一会儿，确保操作系统回收单实例通道
constexpr int kSettleMs = 300;

/**
 * @brief 判断某个进程是否仍在运行
 *
 * Windows 用 OpenProcess + GetExitCodeProcess（不会像 QProcess 那样需要父子关系）；
 * 其它平台走 POSIX 的 kill(pid, 0)。
 */
bool processAlive(qint64 pid)
{
    if (pid <= 0)
        return false;
#ifdef Q_OS_WIN
    const HANDLE handle = ::OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION,
                                        FALSE, static_cast<DWORD>(pid));
    if (!handle)
        return false;   // 打不开通常意味着进程已不存在
    DWORD code = 0;
    const bool alive = ::GetExitCodeProcess(handle, &code) && code == STILL_ACTIVE;
    ::CloseHandle(handle);
    return alive;
#else
    // 需要 <signal.h>；这里用 QProcess 的轻量替代：/proc 存在性判断
    return QFileInfo::exists(QStringLiteral("/proc/%1").arg(pid));
#endif
}

} // namespace

const char *const RestartHelper::kWaitAndRestartFlag = "--restart-wait";

bool RestartHelper::isSupervisorInvocation(const QStringList &arguments)
{
    return arguments.contains(QString::fromLatin1(kWaitAndRestartFlag));
}

int RestartHelper::runSupervisor(const QStringList &arguments)
{
    // 解析 --restart-wait <pid>
    qint64 pid = 0;
    QStringList forward;
    for (int i = 0; i < arguments.size(); ++i) {
        if (arguments.at(i) == QLatin1String(kWaitAndRestartFlag)) {
            if (i + 1 < arguments.size())
                pid = arguments.at(i + 1).toLongLong();
            ++i;   // 跳过 pid
            continue;
        }
        forward << arguments.at(i);
    }

    const QString exePath = QCoreApplication::applicationFilePath();
    const QString workDir = QCoreApplication::applicationDirPath();

    // 等待旧进程结束：这是整个重启流程的关键 —— 旧进程一退出，
    // 它持有的单实例通道才会被操作系统回收，新进程才能成为主实例。
    QElapsedTimer timer;
    timer.start();
    while (pid > 0 && processAlive(pid) && timer.elapsed() < kWaitTimeoutMs)
        QThread::msleep(kPollIntervalMs);

    const bool exited = (pid <= 0) || !processAlive(pid);
    // 再留一点时间给系统回收套接字/命名管道
    QThread::msleep(kSettleMs);

    const bool ok = QProcess::startDetached(exePath, forward, workDir);
    if (ok) {
        Logger::info(QStringLiteral("[监工] 旧进程%1，新实例已启动")
                         .arg(exited ? QStringLiteral("已退出") : QStringLiteral("等待超时")));
    } else {
        Logger::warning(QStringLiteral("[监工] 无法启动新实例: %1").arg(exePath));
    }
    return ok ? 0 : 1;
}

bool RestartHelper::launchRestart(const QStringList &arguments)
{
    const QString exePath = QCoreApplication::applicationFilePath();
    const QString workDir = QCoreApplication::applicationDirPath();

    QStringList supervisorArgs;
    supervisorArgs << QString::fromLatin1(kWaitAndRestartFlag)
                   << QString::number(QCoreApplication::applicationPid())
                   << arguments;

    const bool ok = QProcess::startDetached(exePath, supervisorArgs, workDir);
    if (ok)
        Logger::info(QStringLiteral("已启动重启监工进程（等待本进程退出后拉起新实例）"));
    else
        Logger::critical(QStringLiteral("无法启动重启监工进程: %1").arg(exePath));
    return ok;
}
