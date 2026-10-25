#include "sevensegmentdisplay.h"

#include <QFontMetrics>
#include <QPainter>
#include <QPaintEvent>
#include <QStyleOption>

#include <cmath>

namespace {

/**
 * @brief 七段数码管段码表（下标 = 数字）
 *
 * 段位约定：
 *      aaaa
 *     f    b
 *      gggg
 *     e    c
 *      dddd
 */
const char *const kPatterns[10] = {
    "abcdef",  // 0
    "bc",      // 1
    "abdeg",   // 2
    "abcdg",   // 3
    "bcfg",    // 4
    "acdfg",   // 5
    "acdefg",  // 6
    "abc",     // 7
    "abcdefg", // 8
    "abcdfg"   // 9
};

/// 熄灭段的不透明度：有一点底纹更像真实数码管，但不会喧宾夺主
constexpr int kOffSegmentAlpha = 34;

} // namespace

const char *SevenSegmentDisplay::segmentPattern(int digit)
{
    if (digit < 0 || digit > 9)
        return "";
    return kPatterns[digit];
}

SevenSegmentDisplay::SevenSegmentDisplay(QWidget *parent)
    : QWidget(parent)
{
    // 固定尺寸策略：控件宽度必须刚好等于数字本身所需宽度。
    // 之前用 Preferred/Expanding，布局会把控件拉得比数字宽，多出来的宽度
    // 体现为数字两侧的空白 —— 也就是“数字和左右 Label 间距太大”的根因。
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    // 段是圆角矩形，需要抗锯齿
    setAttribute(Qt::WA_OpaquePaintEvent, false);
}

void SevenSegmentDisplay::display(int value)
{
    if (value < 0)
        value = 0;
    if (m_value == value)
        return;
    m_value = value;
    // 位数可能变化（例如 100 -> 9），控件宽度要跟着调整
    updateGeometry();
    update();
}

void SevenSegmentDisplay::setDigitColor(const QColor &color)
{
    if (!color.isValid() || m_digitColor == color)
        return;
    m_digitColor = color;
    m_offColor = QColor(); // 让熄灭色按新颜色重新推导
    update();
}

void SevenSegmentDisplay::setOffSegmentColor(const QColor &color)
{
    m_offColor = color;
    update();
}

void SevenSegmentDisplay::setAspectRatio(qreal ratio)
{
    if (ratio <= 0.05)
        return;
    m_aspect = ratio;
    updateGeometry();
    update();
}

void SevenSegmentDisplay::setMinimumDigitCount(int count)
{
    count = qMax(0, count);
    if (m_minDigitCount == count)
        return;
    m_minDigitCount = count;
    updateGeometry();
    update();
}

QString SevenSegmentDisplay::glyphs() const
{
    QString text = QString::number(m_value);
    // 不足最小位数时在左侧补空格：数字变化（如 100 -> 99）时整体宽度不变，不会抖动
    while (text.size() < m_minDigitCount)
        text.prepend(QLatin1Char(' '));
    return text;
}

QSize SevenSegmentDisplay::sizeHint() const
{
    const int digits = qMax(1, qMax(m_minDigitCount, QString::number(m_value).size()));
    // 理想高度取 .ui 里设置的最小高度（高考 110 / 一模 90），
    // 宽度严格按“高/宽比”算出刚好容纳整串数字的宽度。
    // 这样布局给到的尺寸与绘制内容一致，数字两侧不会留下多余空白，
    // 整行也就不会因为控件被拉宽而看起来不居中。
    const int h = qMax(minimumHeight(), 40);
    const int gap = qMax(2, h / 16);
    const int digitW = int(std::lround(qreal(h) / m_aspect));
    const int w = digitW * digits + gap * (digits - 1);
    return QSize(qMax(w, minimumWidth()), h);
}

QSize SevenSegmentDisplay::minimumSizeHint() const
{
    // 与 sizeHint 一致：该控件不接受比“刚好容纳数字”更小的尺寸，
    // 否则数字会被压得比设定高度矮（比例虽保持但会显得小）。
    return sizeHint();
}

QRect SevenSegmentDisplay::digitRect() const
{
    const int count = qMax(1, glyphs().size());

    // ============================ 保持原有宽高比 ============================
    // 布局会把控件拉得比数字实际需要的更宽（主页是“大标语 + 数字 + 天”并排，
    // 横向拉伸不可避免）。若让数字跟着控件宽度一起变大，数字就会变得又扁又胖。
    //
    // 正确做法：**以高度为准**推算数字应有的宽度，整串数字再水平居中。
    // 这样无论控件被拉多宽，数字本身始终是固定比例，多出来的宽度只体现为
    // 两侧留白；只有在控件被压得很窄时，才反过来按可用宽度等比收缩。
    // ======================================================================
    const int maxDigitH = height();
    int digitH = maxDigitH;
    int digitW = int(std::lround(digitH / m_aspect));
    int gap = qMax(2, digitH / 16);
    int totalW = digitW * count + gap * (count - 1);

    if (totalW > width()) {
        // 宽度不足：按可用宽度重算，高度随之等比收缩
        const int usable = qMax(1, width());
        digitW = (usable - gap * (count - 1)) / count;
        if (digitW <= 0) {
            // 极端窄：连一个数字都放不下，退化成按宽度均分
            digitW = qMax(1, usable / count);
            gap = 0;
        }
        digitH = int(std::lround(digitW * m_aspect));
        if (digitH > maxDigitH) {
            digitH = maxDigitH;
            digitW = int(std::lround(digitH / m_aspect));
        }
        totalW = digitW * count + gap * (count - 1);
    }
    if (digitW <= 0 || digitH <= 0)
        return QRect();

    const int startX = (width() - totalW) / 2;   // 水平居中（两侧留白等宽）
    const int startY = (height() - digitH) / 2;  // 垂直居中
    return QRect(startX, startY, digitW, digitH);
}

void SevenSegmentDisplay::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QString text = glyphs();
    const int count = qMax(1, text.size());

    // 几何完全来自 digitRect()，与自检用的是同一份计算
    const QRect first = digitRect();
    if (first.isEmpty())
        return;
    const int digitW = first.width();
    const int digitH = first.height();
    const int startX = first.x();
    const int startY = first.y();
    const int gap = (count > 1) ? qMax(2, digitH / 16) : 0;

    // 段厚度：与数字高度成比例，并保证至少 2px，避免小尺寸下段消失
    const int thickness = qMax(2, int(std::lround(digitH * 0.115)));
    const qreal radius = thickness / 2.0;
    const qreal armLen = (digitH - 3.0 * thickness) / 2.0;   // 纵向段长度

    QColor offColor = m_offColor;
    if (!offColor.isValid()) {
        offColor = m_digitColor;
        offColor.setAlpha(kOffSegmentAlpha);
    }

    painter.setPen(Qt::NoPen);

    for (int i = 0; i < count; ++i) {
        const QChar ch = text.at(i);
        const qreal x = startX + i * (digitW + gap);
        const qreal y = startY;

        // 补位空格不画任何东西。
        // 旧实现把空格也当成“没有点亮的数字”，于是把 7 段全部用熄灭色画了一遍 ——
        // 在位数不足时（例如一模已过、只显示“0”）左侧两位是空格，
        // 就会各留下一个完整的暗色“8”轮廓。跳过空格即可消除这种残影，
        // 效果与一直满位显示的高考倒计时保持一致。
        if (!ch.isDigit())
            continue;
        const QString pattern = QString::fromLatin1(segmentPattern(ch.digitValue()));

        // 段几何：横向 a/g/d，纵向 f/b、e/c
        struct Segment { char name; QRectF rect; };
        const Segment segments[7] = {
            { 'a', QRectF(x + thickness,              y,                                  digitW - 2.0 * thickness, thickness) },
            { 'g', QRectF(x + thickness,              y + (digitH - thickness) / 2.0,      digitW - 2.0 * thickness, thickness) },
            { 'd', QRectF(x + thickness,              y + digitH - thickness,              digitW - 2.0 * thickness, thickness) },
            { 'f', QRectF(x,                          y + thickness,                      thickness, armLen) },
            { 'b', QRectF(x + digitW - thickness,     y + thickness,                      thickness, armLen) },
            { 'e', QRectF(x,                          y + thickness + armLen,             thickness, armLen) },
            { 'c', QRectF(x + digitW - thickness,     y + thickness + armLen,             thickness, armLen) }
        };

        for (const Segment &seg : segments) {
            const bool lit = pattern.contains(QLatin1Char(seg.name));
            painter.setBrush(lit ? m_digitColor : offColor);
            painter.drawRoundedRect(seg.rect, radius, radius);
        }
    }
}
