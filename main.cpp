#include "mainwindow.h"
#include "logger.h"

#include <QApplication>
#include <QScreen>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QApplication::setApplicationName(QStringLiteral("BattleForFuture"));
    QApplication::setOrganizationName(QStringLiteral("Explorer"));

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
