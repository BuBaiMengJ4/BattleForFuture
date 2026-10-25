#include "logger.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QMutex>
#include <QMutexLocker>
#include <QTextStream>
#include <QtGlobal>

namespace {

QFile      g_logFile;
QMutex     g_logMutex;          ///< 保护 g_logFile / g_logPath / g_initialized
QString    g_logPath;
bool       g_initialized = false;

/// 防止“写日志 -> 调用 qInfo -> 再次进入消息处理器 -> 再次写日志”的无限递归
thread_local bool t_insideLogger = false;

/// 单条日志的完整格式：时间戳 + 级别 + 内容
QString formatLine(Logger::Level level, const QString &message)
{
    return QStringLiteral("%1 [%2] %3")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz")),
             QLatin1String(Logger::levelName(level)),
             message);
}

/// 把日志级别映射到 Qt 消息级别（仅用于转发给 Qt 默认处理器）
QtMsgType toQtMsgType(Logger::Level level)
{
    switch (level) {
    case Logger::Debug:    return QtDebugMsg;
    case Logger::Info:     return QtInfoMsg;
    case Logger::Warning:  return QtWarningMsg;
    case Logger::Critical: return QtCriticalMsg;
    }
    return QtDebugMsg;
}

/// Qt 消息级别映射到日志级别
Logger::Level fromQtMsgType(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg:    return Logger::Debug;
    case QtInfoMsg:     return Logger::Info;
    case QtWarningMsg:  return Logger::Warning;
    case QtCriticalMsg: return Logger::Critical;
    case QtFatalMsg:    return Logger::Critical;
    }
    return Logger::Debug;
}

} // namespace

bool Logger::initialize(const QString &logDirName)
{
    QMutexLocker locker(&g_logMutex);
    if (g_initialized)
        return true;

    try {
        // 日志目录：<程序运行目录>/Logs，不存在则创建
        const QString appDir = QCoreApplication::applicationDirPath();
        QDir dir(appDir);
        if (!dir.exists(logDirName) && !dir.mkpath(logDirName)) {
            qWarning() << "无法创建日志目录:" << dir.filePath(logDirName);
            return false;
        }

        // 按天分文件，便于归档与排查
        const QString fileName = QStringLiteral("BattleForFuture_%1.log")
                                     .arg(QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd")));
        g_logPath = QDir(dir.filePath(logDirName)).filePath(fileName);

        g_logFile.setFileName(g_logPath);
        if (!g_logFile.open(QIODevice::Append | QIODevice::Text)) {
            qWarning() << "无法打开日志文件:" << g_logPath << g_logFile.errorString();
            g_logPath.clear();
            return false;
        }

        g_initialized = true;
    } catch (...) {
        // 日志初始化失败绝不能导致程序崩溃
        g_logPath.clear();
        return false;
    }

    const QString header = QStringLiteral("======== 日志系统启动，日志文件: %1 ========").arg(g_logPath);
    locker.unlock();

    // 注意：这里必须走 log()，由它统一负责“递归保护 + 先写文件再转发 Qt”。
    // 旧实现在持有互斥锁的情况下直接调用 qInfo()/info()，
    // 一旦消息处理器已被安装就会自锁死（见 log() 注释），
    // 表现为“日志只写了这一行，程序从此没有任何反应”。
    log(Info, header);
    return true;
}

void Logger::shutdown()
{
    QMutexLocker locker(&g_logMutex);
    if (!g_initialized)
        return;

    const QString line = formatLine(Info, QStringLiteral("======== 程序退出，日志系统关闭 ========"));
    if (g_logFile.isOpen()) {
        QTextStream stream(&g_logFile);
        stream.setEncoding(QStringConverter::Utf8);
        stream << line << '\n';
        stream.flush();
    }
    g_logFile.close();
    g_initialized = false;
    g_logPath.clear();
}

void Logger::log(Level level, const QString &message)
{
    // ============================ 死锁修复（关键） ============================
    // 旧实现的问题：
    //   log() 先 QMutexLocker 锁住 g_logMutex（QMutex 默认非递归），
    //   接着调用 qInfo()/qWarning() 输出到控制台；
    //   而 installQtMessageHandler() 已经把这个 qInfo() 转发回 Logger::log()，
    //   于是同一个线程第二次去锁 g_logMutex -> 永久自锁（deadlock）。
    //   现象正是“只写下了『日志系统启动』这一行，之后程序毫无反应、窗口不出现”。
    //
    // 修复方案：
    //   1) 用 thread_local 标记做递归保护，任何重入直接返回，绝不重复加锁；
    //   2) 只在“写文件”这一小段持有互斥锁，写完立刻释放；
    //   3) 控制台输出放到锁外，并且不再经过 Qt 的日志宏（qInfo 等），
    //      而是调用 Qt 默认消息处理器，彻底避免再次回到本函数。
    // =========================================================================
    if (t_insideLogger)
        return;
    t_insideLogger = true;

    const QString line = formatLine(level, message);

    {
        QMutexLocker locker(&g_logMutex);
        if (g_initialized && g_logFile.isOpen()) {
            QTextStream stream(&g_logFile);
            stream.setEncoding(QStringConverter::Utf8);
            stream << line << '\n';
            stream.flush();
        }
    }

    // 控制台输出（保留原有调试习惯；GUI 子系统下没有控制台，输出会被丢弃）
    qt_message_output(toQtMsgType(level), QMessageLogContext(), line);

    t_insideLogger = false;
}

void Logger::installQtMessageHandler()
{
    // 处理器内部只做“格式转换 + 写文件”，不再调用任何 Qt 日志宏，
    // 因此不会与 log() 里的互斥锁形成竞争或递归。
    qInstallMessageHandler([](QtMsgType type, const QMessageLogContext &context, const QString &msg) {
        Q_UNUSED(context);
        Logger::log(fromQtMsgType(type), msg);
    });
}

QString Logger::currentLogFile()
{
    QMutexLocker locker(&g_logMutex);
    return g_logPath;
}

const char *Logger::levelName(Level level)
{
    switch (level) {
    case Debug:    return "DEBUG  ";
    case Info:     return "INFO   ";
    case Warning:  return "WARN   ";
    case Critical: return "ERROR  ";
    }
    return "UNKNOWN";
}

