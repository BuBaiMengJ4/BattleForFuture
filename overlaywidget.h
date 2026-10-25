#ifndef OVERLAYWIDGET_H
#define OVERLAYWIDGET_H

#include <QColor>
#include <QDate>
#include <QWidget>

class QPainter;

/**
 * @brief 桌面小组件样式 2：屏幕右下角的无边框、无标题栏透明倒计时区域
 *
 * 只显示“距离一模/高考还有 N 天”，N 使用 LCD 七段数码管绘制。
 * 窗口本身没有背景、没有标题栏、不占任务栏；**固定停靠在屏幕右下角、不允许拖动**，
 * 双击返回主界面；文字与 LCD 数字颜色均可在设置中自定义。
 *
 * ============================ 为什么不再用 QSS transparent ============================
 * Linux 桌面环境高度碎片化，顶层窗口的“透明”从来不是 QSS 能决定的事：
 * QSS 的 `background: transparent` 只让控件不去绘制自己的背景，顶层窗口的
 * 每一个像素仍然必须由后端填充；X11 下若会话没有合成器（老 GNOME/XFCE 默认、
 * i3/openbox 等纯 WM），后端会把未被绘制的像素直接填成黑色——这就是
 * “QSS 透明变黑”的根因。以下方案彻底绕开这个坑：
 *
 * 1) 支持逐像素 Alpha 的会话（Windows DWM / macOS / Wayland / 带合成器的 X11）：
 *    设置 WA_TranslucentBackground，窗口背景完全不绘制（alpha=0），
 *    只有文字与数码管被画上去，天然透明，并带一层文字描边保证可读性。
 *
 * 2) 不支持逐像素 Alpha 的会话（无合成器的 X11 等）：
 *    不申请透明表面，改为“把内容画到离屏位图 → 用像素级 QRegion 生成窗口遮罩”，
 *    非内容像素直接被系统裁掉，等价于桌面背景透出来，绝不会出现黑块。
 *
 * ============================ 关于字体清晰度 ============================
 * 旧实现在两条路径上都先把内容画到中间 QImage、再贴回窗口，文字被重采样两次，
 * 边缘发虚；同时透明表面上默认开启了子像素抗锯齿，会带来彩色边纹。
 * 现在：
 * - 支持 Alpha 时**直接用 QPainter 画在窗口上**（不经中间图像），边缘最锐利；
 * - 显式关闭子像素抗锯齿，改用干净的灰度抗锯齿；
 * - 不再硬编码字体名，继承系统默认字体，避免字体被静默替换导致字重/字形异常。
 */
class OverlayWidget : public QWidget
{
    Q_OBJECT

public:
    OverlayWidget(const QColor &textColor,
                  const QColor &lcdColor,
                  bool showYiMo,
                  const QDate &gaokaoDate,
                  const QDate &yimoDate,
                  int fontPixelSize = 22,
                  bool alwaysOnTop = true,
                  QWidget *parent = nullptr);

signals:
    /// 用户双击覆盖层：返回主界面
    void returnToMainRequested();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    static int remainingDays(const QDate &targetDate);

    /// 单个七段数码管数字的尺寸（宽、高）
    QSize digitSize() const;

    /// 把内容画到给定 painter 上（paintEvent 与遮罩生成共用同一套绘制逻辑）
    void renderContent(QPainter &painter) const;

    /// 依据渲染结果重建窗口遮罩（仅无逐像素 Alpha 的回退方案使用）
    void rebuildMask();

    bool m_alphaOk = true;           // 是否支持逐像素 Alpha（否则走像素遮罩回退）

    QColor m_textColor;              // 文字颜色（用户自定义）
    QColor m_lcdColor;               // LCD 数字颜色（用户自定义）
    int    m_fontSize = 22;          // 文字字号（px）
    QString m_prefix;                // "距离一模还有" / "距离高考还有"
    int    m_days = 0;               // 剩余天数（最小 0）
    QString m_daysText;              // 剩余天数的十进制字符串（去掉前导零）
};

#endif // OVERLAYWIDGET_H
