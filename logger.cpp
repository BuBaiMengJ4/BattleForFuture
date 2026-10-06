#include "logger.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QMutex>
#include <QMutexLocker>
#include <QTextStream>

namespace {
QFile      g_logFile;
QMutex     g_logMutex;
QString    g_logPath;
bool       g_initialized = false;

/// Qt 消息处理器：把 qDebug/qWarning/... 重定向到日志文件
void qtMessageToLogger(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    Q_UNUSED(context);
    Logger::Level level = Logger::Debug;
    switch (type) {
    case QtDebugMsg:    level = Logger::Debug;    break;
    case QtInfoMsg:     level = Logger::Info;     break;
    case QtWarningMsg:  level = Logger::Warning;  break;
    case QtCriticalMsg: level = Logger::Critical; break;
    case QtFatalMsg:    level = Logger::Critical; break;
    }
    Logger::log(level, msg);
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

    locker.unlock();
    info(QStringLiteral("======== 日志系统启动，日志文件: %1 ========").arg(g_logPath));
    return true;
}

void Logger::shutdown()
{
    QMutexLocker locker(&g_logMutex);
    if (g_initialized) {
        info(QStringLiteral("======== 程序退出，日志系统关闭 ========"));
        g_logFile.close();
        g_initialized = false;
        g_logPath.clear();
    }
}

void Logger::log(Level level, const QString &message)
{
    QMutexLocker locker(&g_logMutex);

    const QString line = QStringLiteral("%1 [%2] %3")
                             .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz")),
                                  QLatin1String(levelName(level)),
                                  message);

    // 控制台输出（保留原有调试习惯）
    switch (level) {
    case Debug:    qDebug().noquote() << line;    break;
    case Info:     qInfo().noquote()  << line;    break;
    case Warning:  qWarning().noquote() << line;  break;
    case Critical: qCritical().noquote() << line; break;
    }

    // 文件输出
    if (g_initialized && g_logFile.isOpen()) {
        QTextStream stream(&g_logFile);
        stream.setEncoding(QStringConverter::Utf8);
        stream << line << '\n';
        stream.flush();
    }
}

void Logger::installQtMessageHandler()
{
    qInstallMessageHandler(qtMessageToLogger);
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
