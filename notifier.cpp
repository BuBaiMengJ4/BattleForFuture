#include "notifier.h"

#include "logger.h"

#include <QAction>
#include <QApplication>
#include <QIcon>
#include <QMenu>
#include <QProcess>
#include <QSystemTrayIcon>

#ifndef Q_OS_WIN
#  include <QStandardPaths>
#endif

namespace {
/// 气泡停留时间（毫秒）
constexpr int kMessageTimeoutMs = 10000;
} // namespace

Notifier &Notifier::instance()
{
    static Notifier notifier;
    return notifier;
}

Notifier::Notifier(QObject *parent)
    : QObject(parent)
{
}

bool Notifier::ensureTray()
{
    if (m_tray)
        return true;

    // 桌面环境可能根本没有托盘（不少 Linux WM 是这样）
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        Logger::warning(QStringLiteral("当前桌面环境不提供系统托盘，无法使用托盘通知"));
        return false;
    }

    m_tray = new QSystemTrayIcon(QIcon(QStringLiteral(":/icon/WindowIcon.ico")), this);
    m_tray->setToolTip(QStringLiteral("BattleForFuture"));

    // 托盘右键菜单：至少给用户一个入口，不至于“只有一个图标点不动”
    auto *menu = new QMenu();
    QAction *showAction = menu->addAction(QObject::tr("显示主界面"));
    QAction *quitAction = menu->addAction(QObject::tr("退出"));
    connect(showAction, &QAction::triggered, this, [this]() { emit showRequested(); });
    connect(quitAction, &QAction::triggered, this, [this]() { emit quitRequested(); });
    m_tray->setContextMenu(menu);

    connect(m_tray, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason) {
                if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick)
                    emit showRequested();
            });

    m_tray->show();
    Logger::debug(QStringLiteral("系统托盘图标已创建（用于推送启动通知）"));
    return true;
}

bool Notifier::notifyViaNotifySend(const QString &title, const QString &body)
{
#ifdef Q_OS_WIN
    Q_UNUSED(title);
    Q_UNUSED(body);
    return false;
#else
    // 只有 Linux 才有 notify-send；先确认可执行文件存在，避免无谓的进程启动
    const QString program = QStandardPaths::findExecutable(QStringLiteral("notify-send"));
    if (program.isEmpty()) {
        Logger::warning(QStringLiteral("未找到 notify-send，无法发送系统通知"));
        return false;
    }
    const bool ok = QProcess::startDetached(program, QStringList{ title, body });
    if (ok)
        Logger::debug(QStringLiteral("已通过 notify-send 发送系统通知"));
    else
        Logger::warning(QStringLiteral("notify-send 调用失败"));
    return ok;
#endif
}

bool Notifier::notify(const QString &title, const QString &body)
{
    if (title.isEmpty() && body.isEmpty())
        return false;

    // 1) 首选：系统托盘气泡（跨平台、无额外依赖）
    if (ensureTray()) {
        m_tray->showMessage(title, body, QSystemTrayIcon::Information, kMessageTimeoutMs);
        Logger::info(QStringLiteral("已通过系统托盘发送通知: %1 / %2").arg(title, body));
        return true;
    }

    // 2) 兜底：Linux 的 notify-send
    if (notifyViaNotifySend(title, body))
        return true;

    Logger::warning(QStringLiteral("当前环境不支持系统通知，已跳过推送"));
    return false;
}

bool Notifier::isAvailable()
{
    if (QSystemTrayIcon::isSystemTrayAvailable())
        return true;
#ifndef Q_OS_WIN
    return !QStandardPaths::findExecutable(QStringLiteral("notify-send")).isEmpty();
#else
    return false;
#endif
}
