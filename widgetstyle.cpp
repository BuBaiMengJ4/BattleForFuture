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

} // namespace

QColor WidgetConfig::parseColor(const QString &value, const QColor &fallback)
{
    if (value.isEmpty())
        return fallback;
    QColor c;
    // 支持 "#rrggbb" 与 "r,g,b" 两种写法
    if (value.startsWith(QLatin1Char('#')))
        c = QColor(value);
    else
        c.setNamedColor(QStringLiteral("#") + value);

    if (!c.isValid() && value.contains(QLatin1Char(','))) {
        const QStringList parts =
            value.split(QLatin1Char(','), Qt::SkipEmptyParts);
        if (parts.size() >= 3) {
            bool okR = false, okG = false, okB = false;
            const int r = parts.at(0).trimmed().toInt(&okR);
            const int g = parts.at(1).trimmed().toInt(&okG);
            const int b = parts.at(2).trimmed().toInt(&okB);
            if (okR && okG && okB)
                c = QColor(qBound(0, r, 255), qBound(0, g, 255), qBound(0, b, 255));
        }
    }
    return c.isValid() ? c : fallback;
}

QString WidgetConfig::colorToString(const QColor &c)
{
    if (!c.isValid())
        return QStringLiteral("255,0,0");
    return QStringLiteral("%1,%2,%3").arg(c.red()).arg(c.green()).arg(c.blue());
}

WidgetConfig WidgetConfig::load(const QString &configPath)
{
    WidgetConfig cfg;
    withSettingsSafe(configPath, [&](QSettings &s) {
        s.beginGroup(QStringLiteral("Widget"));
        cfg.style     = s.value(QStringLiteral("Style"), cfg.style).toInt();
        cfg.showYiMo  = s.value(QStringLiteral("showcontent"), 0).toInt() == 1;
        cfg.bgColor   = parseColor(s.value(QStringLiteral("BackGrandColor")).toString(), cfg.bgColor);
        cfg.textColor = parseColor(s.value(QStringLiteral("TextColor")).toString(), cfg.textColor);
        cfg.lcdColor  = parseColor(s.value(QStringLiteral("LcdColor")).toString(), cfg.lcdColor);
        s.endGroup();
    });
    if (cfg.style != Style1 && cfg.style != Style2)
        cfg.style = Style1;
    return cfg;
}

void WidgetConfig::save(const QString &configPath) const
{
    withSettingsSafe(configPath, [&](QSettings &s) {
        s.beginGroup(QStringLiteral("Widget"));
        s.setValue(QStringLiteral("Style"), style);
        s.setValue(QStringLiteral("showcontent"), showYiMo ? 1 : 0);
        s.setValue(QStringLiteral("BackGrandColor"), colorToString(bgColor));
        s.setValue(QStringLiteral("TextColor"), colorToString(textColor));
        s.setValue(QStringLiteral("LcdColor"), colorToString(lcdColor));
        s.endGroup();
        s.sync();
    });
}
