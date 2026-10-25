#ifndef WIDGETSTYLE_H
#define WIDGETSTYLE_H

#include <QColor>
#include <QString>

/**
 * @brief 小组件配置（样式 / 文字颜色 / LCD 数字颜色 / 背景色 / 显示内容 / 字号）
 *
 * 统一从 config.ini 读写，供桌面小组件（样式 1）、Qt Quick 覆盖层（样式 2）
 * 与设置对话框共用，避免各处散落魔法字符串键名。
 *
 * 兼容性：旧版本使用的小节与键名（Widget/Style、Widget/showcontent、
 * Widget/BackGrandColor、Widget/TextColor、Widget/LcdColor）全部保留，
 * 老 config.ini 可以直接沿用，新字段缺失时使用成员初始值。
 */
struct WidgetConfig
{
    enum Style {
        Style1 = 0, ///< 样式 1：圆角卡片（可自定义背景色）
        Style2 = 1  ///< 样式 2：屏幕右下角无边框透明区域（无标题栏、无背景色）
    };

    int    style     = Style1;
    bool   showYiMo  = false;             ///< true: 显示一模倒计时；false: 显示高考倒计时
    bool   alwaysOnTop = true;            ///< 小组件是否始终置于其它窗口之上（默认开启）
    QColor bgColor   {26, 43, 74};        ///< 背景色（仅样式 1 生效，样式 2 忽略）
    QColor textColor {240, 240, 240};     ///< 文字颜色（两种样式均生效）
    QColor lcdColor  {0, 230, 160};       ///< LCD 数字颜色（两种样式均生效）
    int    fontSize  = 22;                ///< 文字字号（px），两种样式均生效

    /// 位置记忆（仅样式 1 使用；样式 2 固定停靠屏幕右下角，不记忆位置）
    int    posX      = -1;                ///< -1 表示“尚未记录过”，使用默认位置
    int    posY      = -1;

    /// 从 configPath 指向的 ini 文件加载（缺省值即结构体成员初始值）
    static WidgetConfig load(const QString &configPath);
    /// 保存到 ini（立即 sync）
    void save(const QString &configPath) const;

    /// 按 ini 键名设置对应颜色字段（BackGrandColor/TextColor/LcdColor）
    void setColorByKey(const QString &key, const QColor &color);

    /// 解析颜色字符串，支持 "#rrggbb"、"rrggbb"、"r,g,b"、"#aarrggbb"
    static QColor parseColor(const QString &value, const QColor &fallback);
    /// 序列化为 "r,g,b"（沿用旧版本格式，保持向后兼容）
    static QString colorToString(const QColor &c);

    /// 字号取值范围（设置界面滑块/下拉与此保持一致）
    static int minFontSize() { return 12; }
    static int maxFontSize() { return 48; }
    /// 把任意输入收敛到合法字号
    static int clampFontSize(int size) { return qBound(minFontSize(), size, maxFontSize()); }

    /**
     * @brief 读取上次保存的小组件位置（仅样式 1）
     * @return true 表示读到过有效位置；false 表示从未记录过，调用方应使用默认位置
     *
     * 只读写 Widget/PosX、Widget/PosY 两个键，其它字段保持原样，
     * 因此不会因为频繁记录位置而反复重写整份配置。
     */
    static bool loadPosition(const QString &configPath, int *x, int *y);
    /// 记录小组件位置（仅样式 1；样式 2 不调用）
    static void savePosition(const QString &configPath, int x, int y);
    /// 清除位置记录（恢复默认位置时使用）
    static void clearPosition(const QString &configPath);
};

#endif // WIDGETSTYLE_H
