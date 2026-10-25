#ifndef PLATFORMSUPPORT_H
#define PLATFORMSUPPORT_H

#include <QString>

/**
 * @brief 平台能力探测（集中处理 Linux 桌面碎片化带来的差异）
 *
 * Qt 在 Linux 上没有统一的方式查询“当前会话是否支持逐像素 Alpha 合成”：
 * - X11：需要独立的合成器（KWin / Mutter / picom / xfwm4 合成模式），
 *   没有合成器时 WA_TranslucentBackground 的透明像素会被后端填成黑色，
 *   这就是“QSS background: transparent 变成黑块”的根因；
 * - Wayland：表面天然带 Alpha，交给合成器处理，不需要额外探测；
 * - Windows / macOS：DWM / Quartz 始终支持，无需探测。
 *
 * 因此这里统一给出一个可靠的判断，供样式 1（QWidget 圆角卡片）与
 * 样式 2（Qt Quick 透明覆盖层）在创建窗口前决定“走透明路径还是遮罩回退路径”。
 */
namespace PlatformSupport {

/// 会话是否支持逐像素 Alpha（true 时才可以安全使用 WA_TranslucentBackground）
bool supportsPerPixelAlpha();

/// 平台可读名称，用于日志（如 "windows" / "xcb" / "wayland" / "offscreen"）
QString platformName();

} // namespace PlatformSupport

#endif // PLATFORMSUPPORT_H
