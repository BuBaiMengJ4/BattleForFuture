#include "widgetstyle.h"

#include "logger.h"

#include <QSettings>

namespace {

/// 安全读取 ini（异常不外抛）
template <typename Fn>
void withSettingsSafe(const QString &path, Fn &&fn) noexcept
{
    try {
        QSettings settings(path, QSettings::IniFormat);
        fn(settings);
    } catch (...) {
        Logger::critical(QStringLiteral("读写配置文件失败: %1").arg(path));
    }
}

/**
 * @brief 取出配置里颜色键的原始文本
 *
 * 这是“颜色设置怎么改都不生效”的根因修复：
 * INI 语法中逗号是**列表分隔符**，所以当 config.ini 里写的是未加引号的
 *      LcdColor=255,0,0
 * 时，QSettings 会把它解析成 QStringList{"255","0","0"}，
 * 此时 .toString() 返回的是**空字符串**，颜色只能一路回退到默认值
 * （旧版本 config.ini 全都是这种写法）。
 *
 * 因此这里统一处理两种形态：
 * - QString  ：新版本 save() 写出的 "255,0,0"（QSettings 会自动加引号）；
 * - QStringList：老版本 / 手工编辑的未加引号写法，重新用逗号拼回。
 *
 * 同时兼容 "r,g,b"、"#rrggbb"、"rrggbb"、"r,g,b,a" 等输入形式。
 */
QString readColorText(const QSettings &settings, const QString &key)
{
    const QVariant value = settings.value(key);
    if (!value.isValid() || value.isNull())
        return QString();
    if (value.metaType().id() == QMetaType::QStringList)
        return value.toStringList().join(QLatin1Char(','));
    if (value.metaType().id() == QMetaType::QVariantList) {
        QStringList parts;
        const QVariantList list = value.toList();
        parts.reserve(list.size());
        for (const QVariant &item : list)
            parts << item.toString();
        return parts.join(QLatin1Char(','));
    }
    return value.toString();
}

} // namespace

void WidgetConfig::setColorByKey(const QString &key, const QColor &color)
{
    if (!color.isValid())
        return;
    if (key == QLatin1String("BackGrandColor"))
        bgColor = color;
    else if (key == QLatin1String("TextColor"))
        textColor = color;
    else if (key == QLatin1String("LcdColor"))
        lcdColor = color;
}

QColor WidgetConfig::parseColor(const QString &value, const QColor &fallback)
{
    const QString text = value.trimmed();
    if (text.isEmpty())
        return fallback;

    // 1) "#rgb" / "#rrggbb" / "#aarrggbb" / 颜色名
    if (text.startsWith(QLatin1Char('#'))) {
        const QColor c(text);
        if (c.isValid())
            return c;
    } else {
        // 2) 老版本写的是不带 # 的十六进制，如 "ff0000"
        const QColor hexC(QStringLiteral("#") + text);
        if (hexC.isValid())
            return hexC;
    }

    // 3) "r,g,b" / "r,g,b,a"
    if (text.contains(QLatin1Char(','))) {
        const QStringList parts = text.split(QLatin1Char(','), Qt::SkipEmptyParts);
        if (parts.size() >= 3) {
            bool okR = false, okG = false, okB = false, okA = true;
            const int r = parts.at(0).trimmed().toInt(&okR);
            const int g = parts.at(1).trimmed().toInt(&okG);
            const int b = parts.at(2).trimmed().toInt(&okB);
            int a = 255;
            if (parts.size() >= 4)
                a = parts.at(3).trimmed().toInt(&okA);
            if (okR && okG && okB && okA)
                return QColor(qBound(0, r, 255), qBound(0, g, 255),
                              qBound(0, b, 255), qBound(0, a, 255));
        }
    }

    Logger::warning(QStringLiteral("无法解析颜色值 \"%1\"，使用默认值 %2")
                        .arg(value, fallback.name()));
    return fallback;
}

QString WidgetConfig::colorToString(const QColor &c)
{
    if (!c.isValid())
        return QStringLiteral("255,0,0");
    // 保持旧格式 "r,g,b"，避免老版本读到 "#rrggbb" 时解析失败
    return QStringLiteral("%1,%2,%3").arg(c.red()).arg(c.green()).arg(c.blue());
}

WidgetConfig WidgetConfig::load(const QString &configPath)
{
    WidgetConfig cfg;
    withSettingsSafe(configPath, [&](QSettings &s) {
        s.beginGroup(QStringLiteral("Widget"));
        cfg.style     = s.value(QStringLiteral("Style"), cfg.style).toInt();
        cfg.showYiMo  = s.value(QStringLiteral("showcontent"), 0).toInt() == 1;
        // 默认开启：置顶是桌面小组件最符合直觉的行为（默认值 1）
        cfg.alwaysOnTop = s.value(QStringLiteral("AlwaysOnTop"), 1).toInt() != 0;
        // 走 readColorText：兼容未加引号的 "255,0,0"（会被 QSettings 当成列表）
        cfg.bgColor   = parseColor(readColorText(s, QStringLiteral("BackGrandColor")), cfg.bgColor);
        cfg.textColor = parseColor(readColorText(s, QStringLiteral("TextColor")), cfg.textColor);
        cfg.lcdColor  = parseColor(readColorText(s, QStringLiteral("LcdColor")), cfg.lcdColor);
        cfg.fontSize  = clampFontSize(s.value(QStringLiteral("FontSize"), cfg.fontSize).toInt());
        // 位置记忆（仅样式 1 使用）：缺失时保持 -1，表示使用默认位置
        {
            const QVariant vx = s.value(QStringLiteral("PosX"));
            const QVariant vy = s.value(QStringLiteral("PosY"));
            if (vx.isValid() && vy.isValid()) {
                bool okX = false;
                bool okY = false;
                const int px = vx.toInt(&okX);
                const int py = vy.toInt(&okY);
                if (okX && okY) {
                    cfg.posX = px;
                    cfg.posY = py;
                }
            }
        }
        s.endGroup();
    });
    if (cfg.style != Style1 && cfg.style != Style2) {
        Logger::warning(QStringLiteral("配置中的小组件样式 %1 非法，回退为样式 1").arg(cfg.style));
        cfg.style = Style1;
    }
    return cfg;
}

void WidgetConfig::save(const QString &configPath) const
{
    withSettingsSafe(configPath, [&](QSettings &s) {
        s.beginGroup(QStringLiteral("Widget"));
        s.setValue(QStringLiteral("Style"), style);
        s.setValue(QStringLiteral("showcontent"), showYiMo ? 1 : 0);
        s.setValue(QStringLiteral("AlwaysOnTop"), alwaysOnTop ? 1 : 0);
        // 样式 2 不显示背景色，但依然保留该值，方便用户切回样式 1 时恢复
        s.setValue(QStringLiteral("BackGrandColor"), colorToString(bgColor));
        s.setValue(QStringLiteral("TextColor"), colorToString(textColor));
        s.setValue(QStringLiteral("LcdColor"), colorToString(lcdColor));
        s.setValue(QStringLiteral("FontSize"), clampFontSize(fontSize));
        if (posX >= 0 && posY >= 0) {
            s.setValue(QStringLiteral("PosX"), posX);
            s.setValue(QStringLiteral("PosY"), posY);
        }
        s.endGroup();
        s.sync();
    });
}

// ============================= 位置记忆（仅样式 1） =============================

bool WidgetConfig::loadPosition(const QString &configPath, int *x, int *y)
{
    bool found = false;
    withSettingsSafe(configPath, [&](QSettings &s) {
        s.beginGroup(QStringLiteral("Widget"));
        const QVariant vx = s.value(QStringLiteral("PosX"));
        const QVariant vy = s.value(QStringLiteral("PosY"));
        s.endGroup();

        if (!vx.isValid() || !vy.isValid())
            return;

        bool okX = false;
        bool okY = false;
        const int px = vx.toInt(&okX);
        const int py = vy.toInt(&okY);

        // 多显示器时左侧/上方的屏幕坐标本来就为负，所以负值必须放行；
        // 这里只挡掉明显异常的量级，避免把窗口丢到再也点不到的地方。
        constexpr int kSaneLimit = 100000;
        if (okX && okY && qAbs(px) < kSaneLimit && qAbs(py) < kSaneLimit) {
            if (x)
                *x = px;
            if (y)
                *y = py;
            found = true;
        }
    });
    return found;
}

void WidgetConfig::savePosition(const QString &configPath, int x, int y)
{
    // 只动这两个键：避免为了记录一次拖动而重写整份配置
    withSettingsSafe(configPath, [&](QSettings &s) {
        s.beginGroup(QStringLiteral("Widget"));
        s.setValue(QStringLiteral("PosX"), x);
        s.setValue(QStringLiteral("PosY"), y);
        s.endGroup();
        s.sync();
    });
}

void WidgetConfig::clearPosition(const QString &configPath)
{
    withSettingsSafe(configPath, [&](QSettings &s) {
        s.beginGroup(QStringLiteral("Widget"));
        s.remove(QStringLiteral("PosX"));
        s.remove(QStringLiteral("PosY"));
        s.endGroup();
        s.sync();
    });
}
