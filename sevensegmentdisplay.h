#ifndef SEVENSEGMENTDISPLAY_H
#define SEVENSEGMENTDISPLAY_H

#include <QColor>
#include <QSize>
#include <QString>
#include <QWidget>

/**
 * @brief LCD 七段数码管显示控件（替代 QLCDNumber）
 *
 * 为什么不直接用 QLCDNumber？
 * QLCDNumber 会按 `digitCount` 预留内部边距（每个数字格都固定宽度，且四周留白），
 * 结果是控件铺满布局、真正点亮的数字却只有中间一小块 —— 在主页这种
 * “大标语 + LCD 数字”的并排布局里，数字看起来和旁边的 label 文字一样大，
 * 数字周围的空白也显得特别突兀。QLCDNumber 没有公开 API 可以调这些边距。
 *
 * 本控件自己按控件高度计算段几何，**把可用空间完全用满**：
 * 段厚、段长、数字宽度全部由当前尺寸推导，配合 sizeHint 即可得到
 * “数字明显大于文字”的观感；同时段用圆角矩形抗锯齿绘制，不会有锯齿。
 *
 * 用法：
 * @code
 *   display->display(123);              // 显示数值
 *   display->setDigitColor(Qt::red);    // 段颜色
 *   display->setMinimumDigitCount(3);   // 至少 3 位宽（不足补前导空格）
 * @endcode
 */
class SevenSegmentDisplay : public QWidget
{
    Q_OBJECT
    /// 点亮段颜色（可在 .ui 中用 qproperty-digitColor 设置，也可走 QSS 的 color）
    Q_PROPERTY(QColor digitColor READ digitColor WRITE setDigitColor)
    /// 熄灭段颜色（默认取点亮色低透明度，形成淡底纹）
    Q_PROPERTY(QColor offSegmentColor READ offSegmentColor WRITE setOffSegmentColor)
    /// 单个数字的宽高比（高 / 宽）
    Q_PROPERTY(qreal aspectRatio READ aspectRatio WRITE setAspectRatio)

public:
    explicit SevenSegmentDisplay(QWidget *parent = nullptr);

    /// 当前显示的整数值
    int intValue() const { return m_value; }
    /// 期望的显示宽度（位数），不足补空格；0 表示自动
    int minimumDigitCount() const { return m_minDigitCount; }

    QColor digitColor() const { return m_digitColor; }
    QColor offSegmentColor() const { return m_offColor; }
    qreal  aspectRatio() const { return m_aspect; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

public slots:
    /// 显示一个整数（与 QLCDNumber::display 语义一致）
    void display(int value);
    /// 设置段颜色
    void setDigitColor(const QColor &color);
    /// 设置熄灭段颜色（默认由 digitColor 推导）
    void setOffSegmentColor(const QColor &color);
    /// 设置单个数字的宽高比
    void setAspectRatio(qreal ratio);
    /// 设置最小显示位数（用于给 3 位数预留固定宽度，避免数字变化时抖动）
    void setMinimumDigitCount(int count);

    /// 七段段码表（下标 = 数字，字符串中出现的字母即点亮的段）
    static const char *segmentPattern(int digit);

    /**
     * @brief 单个数字占用的矩形（控件坐标，含它在整串数字中的位置）
     *
     * 绘制与自检共用同一份几何计算，避免两处算法漂移。
     * 宽高比保持的关键就在这里：矩形高度尽量占满控件高度，宽度严格按
     * `aspectRatio` 由高度推导，整串数字再整体水平居中。
     * 控件被拉得比内容更宽时，多出来的空间只体现为两侧留白，数字本身不变形。
     */
    QRect digitRect() const;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    /// 需要绘制的字符序列（已按最小位数补空格）
    QString glyphs() const;

    int    m_value = 0;
    int    m_minDigitCount = 0;
    QColor m_digitColor {0, 230, 160};
    QColor m_offColor;                 ///< 无效时按 digitColor 推导
    /// 单个数字的 高/宽 比。
    /// 真实 LCD 数码管是“窄而高”的；早期取值 0.58 意味着 宽 = 1.72×高，
    /// 数字会显得又扁又宽。1.75 让数字明显高瘦，整串宽度也随之收窄。
    qreal  m_aspect = 1.75;
};

#endif // SEVENSEGMENTDISPLAY_H
