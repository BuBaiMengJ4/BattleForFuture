#ifndef OVERLAYWIDGET_H
#define OVERLAYWIDGET_H

#include <QColor>
#include <QDate>
#include <QWidget>

class QQuickWidget;

class QQmlContext;

/**
 * @brief 小组件样式 2：屏幕右下角的无边框、无标题栏透明倒计时覆盖层
 *
 * 采用 Qt Quick（QQuickWidget + 原生 RGBA 表面）实现真透明背景。
 *
 * 为什么不用 QWidget + QSS "background: transparent"？
 * - Linux 桌面环境高度碎片化：X11 下逐像素透明需要合成器（KWin/Xfwm4/picom），
 *   无合成器或老驱动时透明区域会被后端填充为黑色（即“transparent 变黑”问题）；
 *   Wayland 下顶层窗口透明度又取决于合成协议。行为不可控。
 * - Qt Quick 场景由场景图渲染到带 Alpha 通道的表面，未绘制像素写入 alpha=0，
 *   在 Windows(DWM)/Wayland/有合成器的 X11 上表现一致；同时不再依赖旧的
 *   setMask() 位图遮罩 hack，GPU 合成性能更好。
 *
 * 功能与样式 1 保持一致的子集：显示“距离一模/高考还有 N 天”，数字为 LCD 七段码，
 * 支持鼠标拖拽移动、双击返回主界面；文字与 LCD 颜色可由用户自定义。
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
                  QWidget *parent = nullptr);

signals:
    /// 用户双击覆盖层：返回主界面
    void returnToMainRequested();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;




private:
    static int remainingDays(const QDate &targetDate);


    QPoint m_dragPos;               // 按下时鼠标全局位置与窗口左上角的偏移
    bool   m_dragging = false;

    QQuickWidget *m_quick = nullptr;


    QQmlContext  *m_context = nullptr;
    QString m_qmlPath;   // 内嵌 QML 的临时文件路径
};

#endif // OVERLAYWIDGET_H
