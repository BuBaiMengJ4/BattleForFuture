#ifndef LOGGER_H
#define LOGGER_H

#include <QFile>
#include <QString>

/**
 * @brief 轻量级日志系统
 *
 * - 日志文件统一存放在程序运行目录下的 Logs 文件夹中（不存在则自动创建）；
 * - 按天分文件：Logs/BattleForFuture_yyyy-MM-dd.log；
 * - 线程安全（QMutex）；
 * - 同时输出到控制台与文件；
 * - 提供 qInstallMessageHandler 安装函数，让 qDebug/qWarning/qCritical
 *   等 Qt 标准日志宏全部写入日志文件。
 */
class Logger
{
public:
    /// 日志级别
    enum Level { Debug, Info, Warning, Critical };

    /**
     * @brief 初始化日志系统：确保 Logs 目录存在并打开当天日志文件。
     * @param logDirName 日志目录名，默认 "Logs"
     * @return 成功返回 true；失败仅影响日志本身，不影响程序运行。
     */
    static bool initialize(const QString &logDirName = QStringLiteral("Logs"));

    /// 关闭日志文件（程序退出时调用）
    static void shutdown();

    /// 写一条日志
    static void log(Level level, const QString &message);

    static void debug(const QString &msg)   { log(Debug, msg); }
    static void info(const QString &msg)    { log(Info, msg); }
    static void warning(const QString &msg) { log(Warning, msg); }
    static void critical(const QString &msg){ log(Critical, msg); }

    /// 安装为 Qt 消息处理器：qDebug()/qWarning()/qCritical() 自动落盘
    static void installQtMessageHandler();

    /// 当前日志文件完整路径（未初始化时返回空串）
    static QString currentLogFile();

private:
    Logger() = default;
    static const char *levelName(Level level);
};

#endif // LOGGER_H
