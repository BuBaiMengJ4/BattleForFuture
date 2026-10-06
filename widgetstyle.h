#ifndef WIDGETSTYLE_H
#define WIDGETSTYLE_H

#include <QColor>
#include <QDate>
#include <QObject>
#include <QString>

/**
 * @brief 小组件配置（样式 / 文字颜色 / LCD 数字颜色 / 背景色 / 显示内容）
 *
 * 统一从 config.ini 读写，供桌面小组件、Qt Quick 覆盖层与设置对话框共用，
 * 避免各处散落魔法字符串键名。
 */
struct WidgetConfig
{
    enum Style {
        Style1 = 0, ///< 现有样式：圆角卡片 + 标题栏按钮
        Style2 = 1  ///< 屏幕右下角无边框透明区域倒计时
    };

    int     style       = Style1;
    bool    showYiMo    = false;              ///< true: 显示一模倒计时
    QColor  bgColor     {255, 0, 0};          ///< 小组件背景色（仅样式 1 生效）
    QColor  textColor   {240, 240, 240};      ///< 小组件文字颜色（两种样式均生效）
    QColor  lcdColor    {255, 0, 0};          ///< 小组件 LCD 数字颜色（两种样式均生效）

    /// 从 configPath 指向的 ini 文件加载（缺省值即结构体成员初始值）
    static WidgetConfig load(const QString &configPath);
    /// 保存到 ini（立即 sync）
    void save(const QString &configPath) const;

    /// 按 ini 键名设置对应颜色字段（BackGrandColor/TextColor/LcdColor）
    void setColorByKey(const QString &key, const QColor &color);

    /// 解析 "r,g,b" / "#rrggbb" 形式的颜色字符串
    static QColor parseColor(const QString &value, const QColor &fallback);
    /// 序列化为 "r,g,b"
    static QString colorToString(const QColor &c);
};

#endif // WIDGETSTYLE_H
