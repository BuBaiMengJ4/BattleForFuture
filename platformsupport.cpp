#include "platformsupport.h"

#include "logger.h"

#include <QGuiApplication>
#include <QProcessEnvironment>

namespace {

#ifdef Q_OS_LINUX
/// 平台插件名是否属于“自带合成、无需探测”的类别（仅 Linux 分支使用）
bool nameIsAlwaysComposited(const QString &name)
{
    // offscreen / minimal 没有真实窗口，按“支持”处理不会造成黑块
    return name.contains(QLatin1String("wayland"))
        || name.contains(QLatin1String("eglfs"))
        || name.contains(QLatin1String("offscreen"))
        || name.contains(QLatin1String("minimal"));
}
#endif

} // namespace

bool PlatformSupport::supportsPerPixelAlpha()
{
#if !defined(Q_OS_LINUX)
    // Windows(DWM) / macOS(Quartz) / BSD 等：顶层窗口 Alpha 由系统合成器负责
    return true;
#else
    const QString platform = platformName();

    // 非 X11 后端：Wayland、EGLFS、无头平台不依赖外部合成器
    if (nameIsAlwaysComposited(platform))
        return true;

    const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    if (!env.value(QStringLiteral("WAYLAND_DISPLAY")).isEmpty())
        return true; // 用户可能在 X11 会话里用 QT_QPA_PLATFORM=wayland 启动

    if (!platform.contains(QLatin1String("xcb")))
        return true; // 未知后端：保持乐观，交由 Qt 处理

    // ---- X11：探测是否真的有合成管理器在跑 ----
    // X11 下“透明变黑”的根因就是没有合成管理器：
    // WA_TranslucentBackground 的透明像素会被后端直接填成黑色。
    const QString sessionType = env.value(QStringLiteral("XDG_SESSION_TYPE")).toLower();
    const QString desktop = (env.value(QStringLiteral("XDG_CURRENT_DESKTOP"))
                             + QLatin1Char(':') + env.value(QStringLiteral("DESKTOP_SESSION")))
                                .toLower();

    // 会话类型已声明为 Wayland（xcb 只是 XWayland 兼容层）时按支持处理
    if (sessionType == QLatin1String("wayland"))
        return true;

    // 已知默认开启合成的主流桌面
    static const char *const compositedDesktops[] = {
        "kde", "plasma", "gnome", "cinnamon", "deepin", "dde", "ukui",
        "pantheon", "unity", "budgie", "cutefish", "lxqt"
    };
    for (const char *needle : compositedDesktops) {
        if (desktop.contains(QLatin1String(needle)))
            return true;
    }

    // 其余情况（XFCE / LXDE / i3 / openbox / 纯 WM / 未知会话）不做乐观假设：
    // 回退到遮罩方案，宁可牺牲圆角抗锯齿也不出现黑块。
    // 用户若确认自己的 X11 会话有合成器，可设置 BFF_ASSUME_COMPOSITED=1 覆盖。
    if (env.value(QStringLiteral("BFF_ASSUME_COMPOSITED")) == QLatin1String("1"))
        return true;

    Logger::warning(QStringLiteral("X11 会话未检测到合成管理器（平台=%1, 桌面=%2），"
                                   "透明效果将回退为遮罩方案以避免黑块")
                        .arg(platform, desktop));
    return false;
#endif
}

QString PlatformSupport::platformName()
{
    return QGuiApplication::platformName();
}
