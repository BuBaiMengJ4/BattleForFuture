#include "overlaywidget.h"

#include "platformsupport.h"
#include "logger.h"
#include "sevensegmentdisplay.h"
#include "widgetstyle.h"

#include <QApplication>
#include <QBitmap>
#include <QFont>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRegion>
#include <QScreen>

#include <cmath>

namespace {

/// 描边颜色：亮字配深色描边、暗字配浅色描边，保证压在任意壁纸上都能读
QColor outlineColorFor(const QColor &textColor)
{
    return textColor.lightness() > 128 ? QColor(0, 0, 0, 170) : QColor(255, 255, 255, 180);
}

/**
 * @brief 构造适合“透明窗口上绘制文字”的字体与绘制标志
 *
 * 模糊根因说明（本次修复的重点）：
 * 1) 旧实现把内容先画到一张 ARGB 的 QImage，再用 CompositionMode_Source 贴回窗口。
 *    这一进一出让文字被采样两次，边缘必然发软 —— 直接用 QPainter 画在窗口上最锐利；
 * 2) 透明表面上开启子像素抗锯齿（subpixel/LCD 渲染）会引入彩色边纹并且看起来发虚，
 *    因此显式关闭子像素抗锯齿，改用干净的灰度抗锯齿；
 * 3) 字体固定成 "Microsoft YaHei" 时，若系统没有这个字体会被静默替换成别的字体，
 *    字重与 hinting 都不是预期值（Linux 上尤其常见）。这里改为继承控件的字体，
 *    只调整字号与字重，保证与系统一致。
 */
QFont makeLabelFont(const QWidget *widget, int pixelSize)
{
    QFont font = widget ? widget->font() : QApplication::font();
    font.setPixelSize(qMax(8, pixelSize));
    font.setBold(true);
    // 透明表面上必须关掉子像素（LCD）抗锯齿：
    // 子像素渲染依赖不透明的背景色推算次像素，放在半透明/透明表面上
    // 会产生彩色边纹并显得发虚。改用干净的灰度抗锯齿（PreferAntialias）。
    font.setStyleStrategy(QFont::StyleStrategy(QFont::PreferAntialias
                                               | QFont::NoSubpixelAntialias));
    font.setHintingPreference(QFont::PreferDefaultHinting);
    return font;
}

} // namespace

OverlayWidget::OverlayWidget(const QColor &textColor,
                             const QColor &lcdColor,
                             bool showYiMo,
                             const QDate &gaokaoDate,
                             const QDate &yimoDate,
                             int fontPixelSize,
                             bool alwaysOnTop,
                             QWidget *parent)
    : QWidget(parent)
{
    // ---- 顶层窗口：无边框、无标题栏、不占任务栏 ----
    // WindowStaysOnTopHint 由设置里的“始终显示在最上层”控制（默认开启）：
    // 这是一块贴在桌面右下角的信息条，被别的窗口盖住就失去意义，
    // 因此默认置顶；用户若觉得打扰可以在设置里关掉。
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::NoDropShadowWindowHint
                   | (alwaysOnTop ? Qt::WindowStaysOnTopHint : Qt::WindowFlags()));
    setObjectName(QStringLiteral("OverlayWidget"));
    // 样式 2 固定停靠在屏幕右下角，**不允许拖动**：
    // 这是一块纯粹的信息展示区域，位置可预期比“能被拖到任何地方”更有价值，
    // 也避免用户误拖之后找不到它。
    setCursor(Qt::ArrowCursor);

    // ---- 颜色 / 文案 / 天数 ----
    m_textColor = textColor.isValid() ? textColor : QColor(240, 240, 240);
    m_lcdColor  = lcdColor.isValid() ? lcdColor : QColor(0, 230, 160);
    m_fontSize  = WidgetConfig::clampFontSize(fontPixelSize > 0 ? fontPixelSize : 22);

    const bool useYiMo = showYiMo && yimoDate.isValid();
    m_prefix   = useYiMo ? tr("距离一模还有") : tr("距离高考还有");
    m_days     = remainingDays(useYiMo ? yimoDate : gaokaoDate);
    m_daysText = QString::number(m_days);

    // ---- 透明路径选择（详见头文件注释）----
    m_alphaOk = PlatformSupport::supportsPerPixelAlpha();
    if (m_alphaOk)
        setAttribute(Qt::WA_TranslucentBackground);
    else
        setAttribute(Qt::WA_NoSystemBackground);
    // 内容完全由 paintEvent 绘制，避免任何系统背景色渗入（深色主题下即黑色）
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setAutoFillBackground(false);

    // ---- 尺寸：按字号与位数精确计算，避免出现多余的空白区域 ----
    const QFont labelFont = makeLabelFont(this, m_fontSize);
    const QFontMetrics fm(labelFont);
    const int labelWidth  = fm.horizontalAdvance(m_prefix) + fm.horizontalAdvance(tr("天"));
    const int gap         = qMax(8, m_fontSize / 3);
    const int digitW      = digitSize().width();
    const int digitsCount = qMax(1, m_daysText.size());
    const int totalWidth  = labelWidth + gap + digitW * digitsCount + gap;
    const int totalHeight = qMax(digitSize().height(), fm.height());
    setFixedSize(totalWidth + 10, totalHeight + 10);

    // 固定位置：屏幕右下角（样式 2 不记忆位置、不允许拖动）
    if (QScreen *screen = QApplication::primaryScreen()) {
        const QRect avail = screen->availableGeometry();
        move(avail.right() - width() - 20, avail.bottom() - height() - 20);
    }

    // 回退方案需要在窗口映射之前就准备好遮罩，避免首帧闪出黑色矩形
    if (!m_alphaOk)
        rebuildMask();

    Logger::info(QStringLiteral("桌面小组件·样式2（右下角透明区域）已创建：文字 %1 / LCD %2 / 字号 %3 / "
                                "剩余 %4 天 / 透明模式 %5 / 字体 %6")
                     .arg(m_textColor.name(), m_lcdColor.name())
                     .arg(m_fontSize)
                     .arg(m_days)
                     .arg(m_alphaOk ? QStringLiteral("逐像素Alpha")
                                    : QStringLiteral("像素遮罩回退"))
                     .arg(labelFont.family()));
}

int OverlayWidget::remainingDays(const QDate &targetDate)
{
    if (!targetDate.isValid())
        return 0;
    const int days = QDate::currentDate().daysTo(targetDate);
    return days > 0 ? days : 0;
}

QSize OverlayWidget::digitSize() const
{
    // 数码管宽高比固定，保证字形不变形；宽度取偶数，避免半像素模糊
    const int h = qMax(m_fontSize + 10, 30);
    int w = static_cast<int>(std::lround(h * 0.62));
    if (w % 2 != 0)
        ++w;
    return QSize(w, h);
}

// ---------------- 绘制 ----------------

void OverlayWidget::renderContent(QPainter &painter) const
{
    painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing, true);
    // 注意：关闭子像素抗锯齿是通过字体策略（QFont::NoSubpixelAntialias，
    // 见 makeLabelFont）实现的，QPainter 本身没有对应的 RenderHint。

    const QFont labelFont = makeLabelFont(this, m_fontSize);
    painter.setFont(labelFont);

    const QFontMetrics fm(labelFont);
    const QColor outline = outlineColorFor(m_textColor);

    const QSize dSize = digitSize();
    const int gap = qMax(8, m_fontSize / 3);
    const int digitsCount = qMax(1, m_daysText.size());
    const int digitsWidth = dSize.width() * digitsCount;

    const int prefixWidth = fm.horizontalAdvance(m_prefix);
    const int unitWidth   = fm.horizontalAdvance(tr("天"));
    const int contentWidth = prefixWidth + gap + digitsWidth + gap + unitWidth;
    const int left = qMax(0, (width() - contentWidth) / 2);
    const int baseline = (height() + fm.ascent() - fm.descent()) / 2;

    // ---- 前缀/单位文字：描边 + 实心填充 ----
    // 描边用 2px，比旧实现的 2.0 略深一些，避免“发虚”的观感
    auto drawOutlinedText = [&](const QString &text, int x) {
        QPainterPath textPath;
        textPath.addText(QPointF(x, baseline), labelFont, text);
        painter.setPen(QPen(outline, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(textPath);
        painter.fillPath(textPath, m_textColor);
    };
    drawOutlinedText(m_prefix, left);

    // ---- LCD 七段数码管数字 ----
    const int thickness = qMax(3, dSize.height() / 10);
    const int radius    = thickness / 2;

    int x = left + prefixWidth + gap;
    const int y0 = (height() - dSize.height()) / 2;
    const qreal w = dSize.width();
    const qreal h = dSize.height();
    const qreal armLen = (h - 3.0 * thickness) / 2.0; // 纵向段长度
    // 下排竖段从“中间横段的下沿”开始，才能与最下面的横段 d 正好接上
    const qreal lowerArmY = y0 + 2.0 * thickness + armLen;

    painter.setPen(Qt::NoPen);
    for (const QChar ch : m_daysText) {
        const int digit = ch.digitValue();
        const QString pattern = (digit >= 0 && digit <= 9)
                                    ? QString::fromLatin1(SevenSegmentDisplay::segmentPattern(digit))
                                    : QString();

        // 段几何：横向段 a/g/d，纵向段 f/b/e/c
        struct Segment { const char *name; QRectF rect; };
        const Segment segments[] = {
            { "a", QRectF(x + thickness,     y0,                          w - 2.0 * thickness, thickness) },
            { "g", QRectF(x + thickness,     y0 + (h - thickness) / 2.0,  w - 2.0 * thickness, thickness) },
            { "d", QRectF(x + thickness,     y0 + h - thickness,          w - 2.0 * thickness, thickness) },
            { "f", QRectF(x,                 y0 + thickness,              thickness, armLen) },
            { "b", QRectF(x + w - thickness, y0 + thickness,              thickness, armLen) },
            { "e", QRectF(x,                 lowerArmY,                   thickness, armLen) },
            { "c", QRectF(x + w - thickness, lowerArmY,                   thickness, armLen) }
        };

        // 只绘制点亮的段：未点亮的段若也画一层淡色，会在数字上留下“8”的残影
        for (const Segment &seg : segments) {
            if (!pattern.contains(QLatin1String(seg.name)))
                continue;
            painter.setBrush(m_lcdColor);
            painter.drawRoundedRect(seg.rect, radius, radius);
        }
        x += dSize.width();
    }

    // ---- 单位文字 ----
    drawOutlinedText(tr("天"), x + gap);
}

void OverlayWidget::rebuildMask()
{
    // 无逐像素 Alpha 时：用“实际画出来的像素”生成窗口遮罩。
    // 未绘制区域由系统直接裁掉，桌面/壁纸从缺口透出来，绝不会出现黑块；
    // 同时只有文字与数码管参与命中测试。
    QImage canvas(size(), QImage::Format_ARGB32_Premultiplied);
    canvas.fill(Qt::transparent);
    {
        QPainter painter(&canvas);
        renderContent(painter);
    }
    setMask(QRegion(QBitmap::fromImage(
        canvas.createMaskFromColor(Qt::transparent, Qt::MaskInColor))));
}

void OverlayWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    // 关键：直接把内容画在窗口上，**不再经过中间 QImage**。
    // 旧实现“画到离屏图 → 贴回窗口”会让文字被重采样两次，边缘发虚；
    // 这也是样式 2 字体看起来模糊、有锯齿的主要原因。
    QPainter painter(this);
    renderContent(painter);
}

void OverlayWidget::resizeEvent(QResizeEvent *event)
{
    // 尺寸被 setFixedSize 固定，正常不会触发；这里只是保证遮罩与内容永远一致
    if (!m_alphaOk)
        rebuildMask();
    QWidget::resizeEvent(event);
}

// ---------------- 鼠标交互 ----------------
// 样式 2 不允许拖动：把鼠标事件直接交回系统/父级，避免出现“拖不动但光标变了”的错觉。
// 仍然保留双击返回主界面，保证与样式 1 的功能对齐。

void OverlayWidget::mousePressEvent(QMouseEvent *event)
{
    event->ignore();
    QWidget::mousePressEvent(event);
}

void OverlayWidget::mouseMoveEvent(QMouseEvent *event)
{
    event->ignore();
    QWidget::mouseMoveEvent(event);
}

void OverlayWidget::mouseReleaseEvent(QMouseEvent *event)
{
    event->ignore();
    QWidget::mouseReleaseEvent(event);
}

void OverlayWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        Logger::info(QStringLiteral("样式2小组件收到双击，返回主界面"));
        event->accept();
        emit returnToMainRequested();
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}
