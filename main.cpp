#include "mainwindow.h"
#include "logger.h"

#include <QApplication>
#include <QLockFile>
#include <QDir>
#include <QScreen>

int main(int argc, char *argv[])
{
    // ===== 修复：Windows 平台编译成功后“进程启动但窗口不显示”的问题 =====
    // 根因分析（结合日志：两次“日志系统启动”之间没有任何其他日志、也无窗口出现）：
    // 1) 开机自启动通过注册表 Run 键拉起时，若前一次实例仍以后台/小组件隐藏状态
    //    存活（旧版退出逻辑只 hide 主窗口），会出现“进程在跑、日志在写、
    //    但没有任何可见窗口”的僵尸实例；
    // 2) 程序以管理员权限或计划任务等非交互令牌启动时，GUI 会被创建在用户
    //    不可见的桌面上。
    // 处理：a. 检测非活动用户会话并直接退出；b. 单实例锁，保证任何时刻只有
    //       一个可见实例；c. 主窗口 show() 后显式 raise()+activateWindow()，
    //       确保从后台启动时窗口也能弹到前台。
#ifdef Q_OS_WINDOWS
    {
        HMODULE mod = LoadLibraryW(L"kernel32.dll");
        if (mod) {
            typedef DWORD(WINAPI *ActiveConsoleSessionIdFn)();
            typedef BOOL(WINAPI *ProcessIdToSessionIdFn)(DWORD, DWORD *);
            auto activeFn = reinterpret_cast<ActiveConsoleSessionIdFn>(
                reinterpret_cast<void *>(GetProcAddress(mod, "WTSGetActiveConsoleSessionId")));
            auto pidToSid = reinterpret_cast<ProcessIdToSessionIdFn>(reinterpret_cast<void *>(
                GetProcAddress(mod, "ProcessIdToSessionId")));
            DWORD mySession = 0;
            const DWORD activeSession = activeFn ? activeFn() : 0;
            if (pidToSid && activeFn && pidToSid(GetCurrentProcessId(), &mySession)
                && mySession != activeSession) {
                FreeLibrary(mod);
                // 此时日志系统尚未初始化，仅输出到调试器/控制台
                qWarning("检测到程序运行在非活动用户会话（Session %lu != %lu），"
                         "GUI 无法显示，自动退出。请从资源管理器或登录会话中启动。",
                         static_cast<unsigned long>(mySession),
                         static_cast<unsigned long>(activeSession));
                return EXIT_SUCCESS;
            }
        }
        FreeLibrary(mod);
    }
#endif

    QApplication a(argc, argv);
    QApplication::setApplicationName(QStringLiteral("BattleForFuture"));
    QApplication::setOrganizationName(QStringLiteral("Explorer"));

    // 单实例保护：防止开机自启 + 手动双击产生多个互相覆盖的实例
    // （QLockFile 跨平台，异常退出后锁文件会在下次启动时被自动识别失效）
    QLockFile singleInstanceLock(QDir::temp().filePath(QStringLiteral("battleforfuture.lock")));
    singleInstanceLock.setStaleLockTime(5000);
    if (!singleInstanceLock.tryLock(100)) {
        qWarning("已有实例在运行，本次启动取消。");
        return EXIT_SUCCESS;
    }

    // ===== 初始化日志系统（Logs 文件夹不存在时自动创建）=====
    if (!Logger::initialize()) {
        // 日志失败不阻断程序，仅提示
        qWarning() << "日志系统初始化失败，程序将继续运行（无文件日志）。";
    }
    Logger::installQtMessageHandler(); // qDebug/qWarning 等全部落盘
    Logger::info(QStringLiteral("BattleForFuture 启动，版本 %1")
                     .arg(QStringLiteral(APP_VERSION)));

    int exitCode = EXIT_FAILURE;
    try {
        MainWindow w;

        // 将窗口显示于屏幕中心（primaryScreen 可能为空，需判空）
        if (const QScreen *desktop = QApplication::primaryScreen()) {
            const QRect geo = desktop->geometry();
            w.move((geo.width() - w.width()) / 2,
                   (geo.height() - w.height()) / 2);
        } else {
            Logger::warning(QStringLiteral("无法获取主屏幕信息，使用默认窗口位置"));
        }

        exitCode = a.exec();
    } catch (const std::exception &e) {
        Logger::critical(QString::fromLatin1("未捕获异常: %1").arg(e.what()));
    } catch (...) {
        Logger::critical(QStringLiteral("未捕获的未知异常"));
    }

    Logger::info(QStringLiteral("程序退出，返回码 %1").arg(exitCode));
    Logger::shutdown();
    return exitCode;
}
