#include "diagnostics.h"

#include "logger.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QThread>
#include <QTimer>

#include <atomic>

#ifdef Q_OS_WIN
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  include <dbghelp.h>
#endif

namespace {

// ---------------------------- 界面线程心跳 ----------------------------
std::atomic<qint64> g_lastBeatMs{0};
std::atomic<bool>   g_running{true};

// 当前阶段：界面线程写、监视线程读
QMutex  g_stageMutex;
QString g_stage;

#ifdef Q_OS_WIN
/// 界面线程 ID（用于在崩溃日志里判断崩溃是否发生在界面线程上）
std::atomic<DWORD> g_mainThreadId{0};
#endif

QThread  *g_watchThread = nullptr;
QTimer   *g_beatTimer   = nullptr;

/// 卡死判定阈值：界面线程超过这个时间没有心跳就记为疑似卡死
constexpr int kStallThresholdMs = 4000;
/// 同一阶段只报一次，避免刷屏
QString g_lastReportedStage = QStringLiteral("<none>");

} // namespace

// =====================================================================
// 阶段标记
// =====================================================================

QString Diagnostics::currentStage()
{
    QMutexLocker locker(&g_stageMutex);
    return g_stage;
}

void Diagnostics::setStage(const QString &stage)
{
    QMutexLocker locker(&g_stageMutex);
    g_stage = stage;
}

Diagnostics::Step::Step(const QString &stage)
    : m_previous(currentStage())
{
    setStage(stage);
    Logger::debug(QStringLiteral("[阶段] 进入: %1").arg(stage));
}

Diagnostics::Step::~Step()
{
    setStage(m_previous);
    Logger::debug(QStringLiteral("[阶段] 离开: %1").arg(m_previous.isEmpty()
                                                            ? QStringLiteral("<根>")
                                                            : m_previous));
}

// =====================================================================
// 心跳 + 卡死监视
// =====================================================================

void Diagnostics::beat()
{
    g_lastBeatMs.store(QDateTime::currentMSecsSinceEpoch(), std::memory_order_relaxed);
}

#ifdef Q_OS_WIN
namespace {

// ===================== 崩溃捕获（尽量只用原生 API） =====================

/// 崩溃时用 Win32 原语写一行文本，避免依赖 Qt 的内存分配与锁
void rawWriteLine(HANDLE file, const wchar_t *text)
{
    if (!file || file == INVALID_HANDLE_VALUE || !text)
        return;
    DWORD written = 0;
    const DWORD bytes = static_cast<DWORD>(wcslen(text) * sizeof(wchar_t));
    ::WriteFile(file, text, bytes, &written, nullptr);
}

QString g_crashLogPath;   ///< 预先生成，崩溃时直接打开（不做任何分配）

/// 取当前时间的原始文本，时间戳用（失败也不会抛）
void buildTimestamp(wchar_t *buffer, int capacity)
{
    SYSTEMTIME st;
    ::GetLocalTime(&st);
    _snwprintf_s(buffer, capacity, _TRUNCATE,
                 L"%04u-%02u-%02u %02u:%02u:%02u.%03u",
                 st.wYear, st.wMonth, st.wDay,
                 st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
}

LONG WINAPI unhandledExceptionFilter(EXCEPTION_POINTERS *info)
{
    // 1) 基本信息
    const DWORD code = info && info->ExceptionRecord ? info->ExceptionRecord->ExceptionCode : 0;
    const void *address = (info && info->ExceptionRecord)
                              ? info->ExceptionRecord->ExceptionAddress
                              : nullptr;

    // 2) 定位出错模块（哪个 DLL/EXE，偏移多少）——排查时最有用的一条
    wchar_t modulePath[MAX_PATH] = L"<unknown>";
    unsigned long long moduleBase = 0;
    if (address) {
        HMODULE module = nullptr;
        if (::GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                                     | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                 reinterpret_cast<LPCWSTR>(address), &module)
            && module) {
            ::GetModuleFileNameW(module, modulePath, MAX_PATH);
            moduleBase = reinterpret_cast<unsigned long long>(module);
        }
    }

    const HANDLE file = ::CreateFileW(reinterpret_cast<LPCWSTR>(g_crashLogPath.utf16()),
                                      FILE_APPEND_DATA, FILE_SHARE_READ, nullptr,
                                      OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file != INVALID_HANDLE_VALUE) {
        wchar_t line[2048];
        wchar_t stamp[64];
        buildTimestamp(stamp, 64);

        _snwprintf_s(line, 2048, _TRUNCATE,
                     L"\n%ls [ERROR  ] ================= 未处理的结构化异常 =================\n",
                     stamp);
        rawWriteLine(file, line);

        _snwprintf_s(line, 2048, _TRUNCATE,
                     L"%ls [ERROR  ] 异常代码: 0x%08lX    异常地址: %p\n",
                     stamp, code, address);
        rawWriteLine(file, line);

        _snwprintf_s(line, 2048, _TRUNCATE,
                     L"%ls [ERROR  ] 出错模块: %ls (基址 %p, 偏移 0x%llX)\n",
                     stamp, modulePath,
                     reinterpret_cast<void *>(moduleBase),
                     address ? (reinterpret_cast<unsigned long long>(address) - moduleBase) : 0ULL);
        rawWriteLine(file, line);

        // 3) 最后记录到的操作阶段：直接指出卡死/崩溃发生在哪一步
        {
            QMutexLocker locker(&g_stageMutex);
            if (!g_stage.isEmpty()) {
                _snwprintf_s(line, 2048, _TRUNCATE,
                             L"%ls [ERROR  ] 最后阶段: %ls\n", stamp, g_stage.utf16());
                rawWriteLine(file, line);
            }
        }

        // 4) 当前线程与界面线程是否同一个（区分“界面崩溃”与“后台崩溃”）
        const DWORD tid = ::GetCurrentThreadId();
        const DWORD mainTid = g_mainThreadId.load(std::memory_order_relaxed);
        _snwprintf_s(line, 2048, _TRUNCATE,
                     L"%ls [ERROR  ] 线程: 当前=%lu 界面=%lu (%ls)\n",
                     stamp, tid, mainTid, (tid == mainTid ? L"界面线程上崩溃" : L"后台线程上崩溃"));
        rawWriteLine(file, line);

        // 5) 尝试生成 minidump，便于事后加载到调试器看完整调用栈
        if (info) {
            const QString dumpPath = QDir(QFileInfo(g_crashLogPath).absolutePath())
                                         .filePath(QStringLiteral("dumps"));
            QDir().mkpath(dumpPath);
            const QString dumpFile = QDir(dumpPath).filePath(
                QStringLiteral("crash_%1.dmp")
                    .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"))));

            const HANDLE dump = ::CreateFileW(reinterpret_cast<LPCWSTR>(dumpFile.utf16()),
                                              GENERIC_WRITE, 0, nullptr,
                                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (dump != INVALID_HANDLE_VALUE) {
                MINIDUMP_EXCEPTION_INFORMATION mei;
                mei.ThreadId = ::GetCurrentThreadId();
                mei.ExceptionPointers = info;
                mei.ClientPointers = FALSE;
                const BOOL ok = ::MiniDumpWriteDump(::GetCurrentProcess(), ::GetCurrentProcessId(),
                                                    dump, MiniDumpWithDataSegs, &mei, nullptr, nullptr);
                ::CloseHandle(dump);
                _snwprintf_s(line, 2048, _TRUNCATE,
                             L"%ls [ERROR  ] minidump: %ls (%ls)\n",
                             stamp, dumpFile.utf16(), ok ? L"已生成" : L"生成失败");
                rawWriteLine(file, line);
            }
        }

        _snwprintf_s(line, 2048, _TRUNCATE,
                     L"%ls [ERROR  ] ==========================================================\n",
                     stamp);
        rawWriteLine(file, line);
        ::FlushFileBuffers(file);
        ::CloseHandle(file);
    }

    // 交回系统默认处理，保持原有的崩溃行为（不吞掉异常）
    return EXCEPTION_CONTINUE_SEARCH;
}

} // namespace
#endif // Q_OS_WIN

// =====================================================================
// 安装 / 卸载
// =====================================================================

void Diagnostics::install()
{
    // ---- 1) 主线程心跳定时器（属于界面线程） ----
    if (!g_beatTimer) {
        g_beatTimer = new QTimer();
        g_beatTimer->setInterval(500);
        QObject::connect(g_beatTimer, &QTimer::timeout, qApp, []() { Diagnostics::beat(); });
        g_beatTimer->start();
    }
    beat();

#ifdef Q_OS_WIN
    g_mainThreadId.store(::GetCurrentThreadId(), std::memory_order_relaxed);

    // 预先生成崩溃日志路径：崩溃时不能再做任何字符串拼接
    const QString logDir = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("Logs"));
    QDir().mkpath(logDir);
    g_crashLogPath = QDir(logDir).filePath(QStringLiteral("BattleForFuture_crash.log"));

    ::SetUnhandledExceptionFilter(unhandledExceptionFilter);
    Logger::debug(QStringLiteral("已启用崩溃捕获（未处理异常将写入 %1）").arg(g_crashLogPath));
#else
    Logger::debug(QStringLiteral("当前平台未启用结构化异常捕获（仅提供卡死监视）"));
#endif

    // ---- 2) 卡死监视线程 ----
    if (!g_watchThread) {
        g_running.store(true);
        g_watchThread = QThread::create([]() {
            while (g_running.load(std::memory_order_relaxed)) {
                QThread::sleep(1);

                const qint64 last = g_lastBeatMs.load(std::memory_order_relaxed);
                if (last == 0)
                    continue;
                const qint64 stallMs = QDateTime::currentMSecsSinceEpoch() - last;
                if (stallMs < kStallThresholdMs)
                    continue;

                const QString stage = currentStage();
                {
                    QMutexLocker locker(&g_stageMutex);
                    if (stage == g_lastReportedStage)
                        continue; // 同一阶段只报一次
                    g_lastReportedStage = stage;
                }

                Logger::critical(QStringLiteral("检测到界面线程疑似卡死：已阻塞 %1 ms，最后阶段: %2")
                                     .arg(stallMs)
                                     .arg(stage.isEmpty() ? QStringLiteral("<未记录>") : stage));
            }
        });
        g_watchThread->setObjectName(QStringLiteral("DiagnosticsWatcher"));
        g_watchThread->start();
        Logger::debug(QStringLiteral("已启用界面卡死监视（阈值 %1 ms）").arg(kStallThresholdMs));
    }
}

void Diagnostics::shutdown()
{
    g_running.store(false);
    if (g_watchThread) {
        g_watchThread->wait(2000);
        delete g_watchThread;
        g_watchThread = nullptr;
    }
    if (g_beatTimer) {
        g_beatTimer->stop();
        delete g_beatTimer;
        g_beatTimer = nullptr;
    }
#ifdef Q_OS_WIN
    ::SetUnhandledExceptionFilter(nullptr);
#endif
}
