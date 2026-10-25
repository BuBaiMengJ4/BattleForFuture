#include "appinstance.h"
#include "diagnostics.h"
#include "logger.h"
#include "mainwindow.h"
#include "restarthelper.h"
#include "textutils.h"
#include "version.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QLibraryInfo>
#include <QList>
#include <QScreen>
#include <QSettings>
#include <QStringConverter>
#include <QSurfaceFormat>
#include <QTimer>
#include <QTranslator>

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

/**
 * @brief Windows：确认当前进程运行在“活动用户会话”
 *
 * 通过计划任务 / 服务 / 提权工具启动时，进程可能落在会话 0 或已断开的会话里，
 * 此时 GUI 会被创建在用户看不见的桌面上——表现就是“编译成功、进程在跑、
 * 日志在写，但窗口永远不出现”。这种情况直接退出并给出明确提示，比默默无闻更好。
 *
 * 注意：必须包含 <windows.h>，否则 DWORD/BOOL/GetProcAddress 等都不可见
 * （这正是之前 main.cpp 编译失败的根因）。
 */
#ifdef Q_OS_WIN
bool runningInNonInteractiveSession()
{
    const HMODULE kernel32 = ::LoadLibraryW(L"kernel32.dll");
    if (!kernel32)
        return false;

    using ActiveConsoleSessionIdFn = DWORD(WINAPI *)();
    using ProcessIdToSessionIdFn   = BOOL(WINAPI *)(DWORD, DWORD *);

    auto activeConsoleSessionId = reinterpret_cast<ActiveConsoleSessionIdFn>(
        reinterpret_cast<void *>(::GetProcAddress(kernel32, "WTSGetActiveConsoleSessionId")));
    auto processIdToSessionId = reinterpret_cast<ProcessIdToSessionIdFn>(
        reinterpret_cast<void *>(::GetProcAddress(kernel32, "ProcessIdToSessionId")));

    bool mismatched = false;
    if (activeConsoleSessionId && processIdToSessionId) {
        DWORD mySession = 0;
        const DWORD activeSession = activeConsoleSessionId();
        if (activeSession != 0xFFFFFFFFu
            && processIdToSessionId(::GetCurrentProcessId(), &mySession)
            && mySession != activeSession) {
            mismatched = true;
            qWarning("检测到程序运行在非活动用户会话（Session %lu != %lu），"
                     "GUI 无法显示，本次启动自动退出。请从资源管理器或登录会话中启动。",
                     static_cast<unsigned long>(mySession),
                     static_cast<unsigned long>(activeSession));
        }
    }

    ::FreeLibrary(kernel32);
    return mismatched;
}
#endif

/**
 * @brief 在创建 QApplication 之前请求带 Alpha 的默认表面格式
 *
 * 这是修复“Linux 桌面下 QSS transparent 变黑”的前置条件：
 * 只有默认表面本身带 Alpha 通道，WA_TranslucentBackground 才有意义。
 * 必须在 QApplication 构造前设置，之后再改对已创建的平台窗口无效。
 * Windows(DWM) 等平台即使拿到 Alpha 表面也没有副作用。
 */
void requestAlphaCapableSurface()
{
    QSurfaceFormat format = QSurfaceFormat::defaultFormat();
    if (format.alphaBufferSize() <= 0)
        format.setAlphaBufferSize(8);
    // 小组件是长期驻留的轻量窗口，交换间隔保持默认即可
    QSurfaceFormat::setDefaultFormat(format);
}

/**
 * @brief 繁体翻译器：把翻译结果里的简体文本统一转成繁体
 *
 * 详见 textutils.h 的说明：源码里的文案都是简体，与其维护两份 .ts，
 * 不如在翻译器出口做一次字符级转换，新增文案自动就有繁体版本。
 * 同时它也让 Qt 内置的“Qt 自带控件翻译”（如颜色对话框）一起变成繁体。
 */
class TraditionalTranslator : public QTranslator
{
public:
    explicit TraditionalTranslator(QObject *parent = nullptr) : QTranslator(parent) {}

    bool isEmpty() const override { return false; }

    QString translate(const char *context, const char *sourceText,
                      const char *disambiguation = nullptr, int n = -1) const override
    {
        // 1) 先问被包装的翻译器（可以为空，为空时直接用源字符串）
        QString result;
        if (m_inner)
            result = m_inner->translate(context, sourceText, disambiguation, n);
        if (result.isEmpty() && sourceText)
            result = QString::fromUtf8(sourceText);

        // 2) 统一转成繁体后返回
        return TextUtils::toTraditional(result);
    }

    void setInner(QTranslator *inner) { m_inner = inner; }
    /// 声明是否由本对象负责销毁内层翻译器
    void setOwnsInner(bool owns) { m_ownsInner = owns; }
    ~TraditionalTranslator() override
    {
        if (m_ownsInner)
            delete m_inner;
    }

private:
    QTranslator *m_inner = nullptr;
    bool         m_ownsInner = false;
};

/**
 * @brief 加载界面翻译
 *
 * 语言索引（与设置界面的下拉框一致）：
 *   0 = 简体中文（源码原文，不需要翻译器）
 *   1 = 繁體中文（在简体基础上做运行时转换）
 *
 * 另外无论选哪种中文，都会加载 Qt 自带的 qt_zh_CN 翻译，
 * 否则 QColorDialog / QMessageBox 这些标准对话框的按钮与标签仍是英文
 * ——这正是“颜色设置对话框是英文”的原因。
 *
 * @param translators 用于保管翻译器对象的容器（必须是长生命周期对象）
 * @param languageIndex 语言索引
 */
void installTranslators(QList<QTranslator *> &translators, int languageIndex)
{
    for (QTranslator *translator : translators) {
        QCoreApplication::removeTranslator(translator);
        delete translator;
    }
    translators.clear();

    // ---- 1) Qt 内置控件的中文翻译（颜色对话框、消息框等）----
    // 资源里已经内嵌 qt_zh_CN.qm；同时兼容从 Qt 安装目录加载（开发环境）。
    auto *qtTranslator = new QTranslator();
    bool qtLoaded = qtTranslator->load(QStringLiteral(":/i18n/qt_zh_CN.qm"));
    if (!qtLoaded) {
        qtLoaded = qtTranslator->load(QStringLiteral("qt_zh_CN"),
                                      QLibraryInfo::path(QLibraryInfo::TranslationsPath));
    }
    if (qtLoaded) {
        // 注意：繁体中**不能**直接把 qtTranslator 装上去。
        // 它是“简体”翻译，直接安装会让颜色对话框在繁体模式下显示简体；
        // 而如果像旧实现那样在繁体分支里完全不安装它（或把它装在
        // TraditionalTranslator 之后），标准对话框就会退回英文 —— 这正是
        // “繁体模式下 ColorDialog 是英文”的根因。
        //
        // 正确做法见下面 languageIndex > 0 的分支：
        // 把 qtTranslator 作为 TraditionalTranslator 的“内层”翻译器，
        // 让它提供的简体结果再经过一次简 -> 繁转换。
        if (languageIndex <= 0) {
            QCoreApplication::installTranslator(qtTranslator);
            translators.append(qtTranslator);
        }
        Logger::debug(QStringLiteral("已加载 Qt 内置中文翻译（颜色对话框等标准控件）"));
    } else {
        Logger::warning(QStringLiteral("未能加载 Qt 内置中文翻译 qt_zh_CN.qm，"
                                       "标准对话框将显示英文"));
    }

    if (languageIndex <= 0) {
        if (!qtLoaded)
            delete qtTranslator;
        Logger::info(QStringLiteral("界面语言：简体中文（源码原文）"));
        return;
    }

    // ---- 2) 繁体：用一个转换器包住 Qt 翻译，统一输出繁体 ----
    // 应用自身文案是简体源码，Qt 内置文案来自 qt_zh_CN.qm（也是简体），
    // 两者都交给 TraditionalTranslator 转换，因此繁体模式下不会出现
    // 简体残留，也不会退回英文。
    auto *traditional = new TraditionalTranslator();
    traditional->setInner(qtLoaded ? qtTranslator : nullptr);
    traditional->setOwnsInner(qtLoaded);
    QCoreApplication::installTranslator(traditional);
    translators.append(traditional);
    Logger::info(QStringLiteral("界面语言：繁體中文（运行时转换，含标准对话框）"));
}

/**
 * @brief 读取配置中的界面语言索引
 */
int readLanguageIndex()
{
    QSettings settings(QDir(QCoreApplication::applicationDirPath())
                           .filePath(QStringLiteral("config.ini")),
                       QSettings::IniFormat);
    return settings.value(QStringLiteral("Global/language"), 0).toInt();
}

} // namespace

int main(int argc, char *argv[])
{
#ifdef Q_OS_WIN
    if (runningInNonInteractiveSession())
        return EXIT_SUCCESS;
#endif

    requestAlphaCapableSurface();

    QApplication a(argc, argv);
    QApplication::setApplicationName(QStringLiteral("BattleForFuture"));
    QApplication::setOrganizationName(QStringLiteral("Explorer"));
    QApplication::setApplicationVersion(QStringLiteral(APP_VERSION));
    QApplication::setQuitOnLastWindowClosed(true);

    // ===== 日志系统（Logs 文件夹不存在时自动创建）=====
    if (!Logger::initialize())
        qWarning() << "日志系统初始化失败，程序将继续运行（无文件日志）。";
    Logger::installQtMessageHandler();
    Diagnostics::install();   // 崩溃捕获 + 界面卡死监视（必须在日志之后）

    // ===== 重启监工模式（必须早于 AppInstance）=====
    // 监工是同一个可执行文件的另一个进程。如果它先占用了单实例通道，
    // 随后被它拉起的新实例就会变成“第二实例”而立即退出。
    // 监工只做两件事：等旧进程结束 -> 启动新的 GUI 实例，然后自己退出。
    if (RestartHelper::isSupervisorInvocation(QCoreApplication::arguments())) {
        const int rc = RestartHelper::runSupervisor(QCoreApplication::arguments().mid(1));
        Logger::shutdown();
        return rc;
    }

    Logger::info(QStringLiteral("BattleForFuture 启动，版本 %1，平台 %2")
                     .arg(QStringLiteral(APP_VERSION), QGuiApplication::platformName()));

    // ===== 界面翻译（必须在创建窗口之前安装）=====
    QList<QTranslator *> translators;
    installTranslators(translators, readLanguageIndex());

    // ===== 单实例：已有实例在运行时，把它的窗口带到前台而不是静默退出 =====
    AppInstance instance(QStringLiteral("BattleForFuture.SingleInstance"));
    if (!instance.tryBecomePrimary()) {
        Logger::info(QStringLiteral("本次启动已交给正在运行的实例处理"));
        Logger::shutdown();
        return EXIT_SUCCESS;
    }

    int exitCode = EXIT_FAILURE;
    try {
        MainWindow window;

        // 已有实例收到显示请求时，把窗口抬到前台
        QObject::connect(&instance, &AppInstance::showRequested, &window, &MainWindow::bringToFront);

        // 启动形态：配置项 Start/StartAsWidget = true 表示直接进入桌面小组件。
        // 默认（配置缺失或首次运行）进入主窗口，保证用户启动后一定看到界面。
        bool startAsWidget = false;
        {
            QSettings settings(QDir(QCoreApplication::applicationDirPath())
                                   .filePath(QStringLiteral("config.ini")),
                               QSettings::IniFormat);
            startAsWidget = settings.value(QStringLiteral("Start/StartAsWidget"), false).toBool();
        }

        // 延迟到事件循环开始后再显示：此时窗口已完全构造、平台窗口已创建，
        // raise()/activateWindow() 才能真正生效（Windows 上尤其明显）。
        if (startAsWidget) {
            Logger::info(QStringLiteral("根据配置以桌面小组件模式启动"));
            QTimer::singleShot(0, &window, [&window]() {
                window.enterWidgetMode();
                window.bringToFront();
            });
        } else {
            Logger::info(QStringLiteral("以主窗口模式启动"));
            QTimer::singleShot(0, &window, [&window]() {
                if (const QScreen *screen = QApplication::primaryScreen()) {
                    const QRect available = screen->availableGeometry();
                    window.move(available.center() - window.rect().center());
                }
                window.bringToFront();
            });
        }

        exitCode = a.exec();
    } catch (const std::exception &e) {
        Logger::critical(QString::fromLatin1("未捕获异常: %1").arg(QString::fromUtf8(e.what())));
    } catch (...) {
        Logger::critical(QStringLiteral("未捕获的未知异常"));
    }

    Logger::info(QStringLiteral("程序退出，返回码 %1").arg(exitCode));
    Diagnostics::shutdown();
    Logger::shutdown();
    return exitCode;
}
