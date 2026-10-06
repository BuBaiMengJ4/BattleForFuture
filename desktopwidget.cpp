#include "desktopwidget.h"
#include "logger.h"

#include <QLCDNumber>
#include <QLabel>
#include <QPushButton>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRegion>
#include <QScreen>
#include <QApplication>
#include <QFont>
#include <QStyle>

#include <cmath>

DesktopWidget::DesktopWidget(const QColor &backgroundColor,
                             const QColor &textColor,
                             const QColor &lcdColor,
                             bool showYiMo,
                             const QDate &gaokaoDate,
                             const QDate &yimoDate,
                             QWidget *parent)
    : QDialog(parent)
{
    // 无边框 + Tool：不占任务栏，可当桌面小组件
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool);
    setObjectName(QStringLiteral("DesktopWidget"));
    setFixedSize(250, 200);
    setCursor(Qt::SizeAllCursor); // 提示整体可拖动

    // ---- Linux 碎片化兼容：检测逐像素 Alpha 是否可用 ----
    // X11 下若会话没有合成器（老 GNOME/XFCE 默认、部分精简 WM），
    // WA_TranslucentBackground 的透明区域会被渲染成黑色（即 “QSS transparent 变黑”）。
    // 此时回退为“不透明背景 + setMask 圆角遮罩”，保证任何环境下都不出现黑块。
#if defined(Q_OS_LINUX)
    {
        const bool rgbaVisual = QApplication::primaryScreen()
                                && QApplication::primaryScreen()->hasAlphaChannel();
        m_alphaOk = rgbaVisual; // Wayland 表面天然支持 alpha；X11 取决于合成器视觉
        if (!m_alphaOk) {
            Logger::warning(QStringLiteral("当前 Linux 会话不支持逐像素 Alpha（无合成器？），"
                                           "小组件回退为遮罩圆角方案以避免透明变黑"));
        }
    }
#endif
    if (m_alphaOk)
        setAttribute(Qt::WA_TranslucentBackground);

    // ---- 颜色配置 ----
    m_bgColor   = backgroundColor.isValid() ? backgroundColor : QColor(255, 0, 0);
    m_textColor = textColor.isValid() ? textColor : QColor(240, 240, 240);
    m_lcdColor  = lcdColor.isValid()  ? lcdColor  : QColor(255, 0, 0);

    // ---- 倒计时内容 ----
    QFont titleFont(QStringLiteral("宋体"), 20);

    m_titleLabel = new QLabel(tr("还有"), this);
    m_titleLabel->setFont(titleFont);
    m_titleLabel->setStyleSheet(QStringLiteral("color: %1; background: transparent;")
                                    .arg(m_textColor.name()));
    m_titleLabel->setGeometry(10, 0, 80, 30);

    m_unitLabel = new QLabel(this);
    m_unitLabel->setFont(titleFont);
    m_unitLabel->setStyleSheet(QStringLiteral("color: %1; background: transparent;")
                                   .arg(m_textColor.name()));
    m_unitLabel->setGeometry(70, 140, 160, 30);

    m_lcd = new QLCDNumber(this);
    m_lcd->move(30, 30);
    m_lcd->setDigitCount(3);
    m_lcd->setSegmentStyle(QLCDNumber::Flat);
    m_lcd->setFrameShape(QFrame::NoFrame);
    m_lcd->resize(200, 100);
    // LCD 数字颜色由用户自定义；背景保持透明（画在 paintEvent 的圆角背景之上）
    m_lcd->setStyleSheet(QStringLiteral("color: %1; background: transparent;")
                             .arg(m_lcdColor.name()));

    if (showYiMo && yimoDate.isValid()) {
        m_lcd->display(remainingDays(yimoDate));
        m_unitLabel->setText(tr("天到一模"));
    } else {
        m_lcd->display(remainingDays(gaokaoDate));
        m_unitLabel->setText(tr("天到高考"));
    }

    // ---- 按钮（文字颜色跟随用户设置，背景用可读性自适应色）----
    const QString btnStyle = QStringLiteral(
        "QPushButton {"
        "   background-color: rgba(255,255,255,30);"
        "   color: %1;"
        "   border: 1px solid rgba(255,255,255,60);"
        "   border-radius: 4px;"
        "}"
        "QPushButton:hover { background-color: rgba(255,255,255,60); }")
        .arg(m_textColor.name());

    m_exitBtn = new QPushButton(tr("退出"), this);
    m_exitBtn->setGeometry(10, 179, 50, 20);
    m_exitBtn->setStyleSheet(btnStyle);
    m_returnBtn = new QPushButton(tr("返回主界面"), this);
    m_returnBtn->setGeometry(140, 179, 100, 20);
    m_returnBtn->setStyleSheet(btnStyle);

    connect(m_exitBtn, &QPushButton::clicked, qApp, &QApplication::quit);
    connect(m_returnBtn, &QPushButton::clicked, this, &DesktopWidget::returnToMainRequested);

    // ---- 背景色（画在 paintEvent 中，样式表对顶层无边框窗口的 border-radius 无效）----

    // 初始位置：屏幕右上角（保持原有习惯，之后可用鼠标拖到任意位置）
    if (QScreen *desktop = QApplication::primaryScreen()) {
        const QRect avail = desktop->availableGeometry();
        move(avail.right() - width(), avail.top());
    }

    Logger::info(QStringLiteral("桌面小组件·样式1已创建（文字 %1 / LCD %2 / 背景 %3）")
                     .arg(m_textColor.name(), m_lcdColor.name(), m_bgColor.name()));
}

int DesktopWidget::remainingDays(const QDate &targetDate)
{
    if (!targetDate.isValid())
        return 0;
    const int days = QDate::currentDate().daysTo(targetDate);
    return days > 0 ? days : 0;
}

// ---------------- 拖拽移动 ----------------
void DesktopWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        // 记录鼠标全局位置与窗口左上角的偏移，保证拖动跟手
        m_dragPos = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
        return;
    }
    QDialog::mousePressEvent(event);
}

void DesktopWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging && (event->buttons() & Qt::LeftButton)) {
        QPoint newPos = event->globalPosition().toPoint() - m_dragPos;

        // 限制在屏幕范围内，防止拖丢
        if (QScreen *screen = QApplication::primaryScreen()) {
            const QRect avail = screen->availableGeometry();
            newPos.setX(qBound(avail.left(), newPos.x(), avail.right() - width()));
            newPos.setY(qBound(avail.top(), newPos.y(), avail.bottom() - height()));
        }
        move(newPos);
        event->accept();
        return;
    }
    QDialog::mouseMoveEvent(event);
}

void DesktopWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_dragging) {
        m_dragging = false;
        Logger::debug(QStringLiteral("小组件移动到 (%1, %2)").arg(x()).arg(y()));
        event->accept();
        return;
    }
    QDialog::mouseReleaseEvent(event);
}

// ---------------- 真圆角 ----------------
void DesktopWidget::rebuildMask()
{
    // 用几何 Region 做窗口遮罩：圆角之外的像素被系统直接裁掉，
    // 配合 WA_TranslucentBackground 得到“真正的圆角”（无阴影、无方角残留）。
    // 相比逐像素扫描位图生成遮罩，这里直接用矩形构造，性能更好。
    const int r = m_radius;

    QRegion mask;
    // 中间大矩形（去掉上下两个圆角条带）
    mask += QRect(0, r, width(), height() - 2 * r);
    // 上下一条“内缩”矩形，覆盖圆角条带中部的绝大部分
    mask += QRect(r, 0, width() - 2 * r, r);
    mask += QRect(r, height() - r, width() - 2 * r, r);
    // 四个角落用近似四分之一圆的阶梯扫描线补齐
    auto cornerRegion = [r](int cx, int cy, bool left, bool top) {
        QRegion region;
        for (int dy = 0; dy < r; ++dy) {
            // 以圆心 (cx, cy) 计算每行的最大横向延伸
            const int dx = int(std::sqrt(qreal(r * r - dy * dy)));
            const int x0 = left ? cx - dx : cx;
            const int w  = dx + 1;
            const int y  = top ? cy + dy : cy - dy - 1;
            region += QRect(x0, y, w, 1);
        }
        return region;
    };
    mask += cornerRegion(r, r, true, true);                                    // 左上
    mask += cornerRegion(width() - r - 1, r, false, true);                     // 右上
    mask += cornerRegion(r, height() - r - 1, true, false);                    // 左下
    mask += cornerRegion(width() - r - 1, height() - r - 1, false, false);     // 右下

    setMask(mask);
}

void DesktopWidget::resizeEvent(QResizeEvent *event)
{
    if (!m_alphaOk)
        rebuildMask(); // 仅在无逐像素 Alpha 的回退方案下需要遮罩
    QDialog::resizeEvent(event);
}

void DesktopWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QPen borderPen(QColor(224, 208, 160), 1); // 保留原 1px 描边
    borderPen.setCapStyle(Qt::RoundCap);
    painter.setPen(borderPen);

    if (m_alphaOk) {
        // 正常路径：抗锯齿圆角矩形，四角因逐像素 Alpha 而真正透明（无阴影）
        QPainterPath path;
        path.addRoundedRect(rect().adjusted(0, 0, -1, -1), m_radius, m_radius);
        painter.fillPath(path, m_bgColor);      // 圆角背景色
        painter.drawPath(path);                 // 描边
    } else {
        // 回退路径（无合成器的 Linux 会话）：不透明填充整窗，
        // 圆角由 setMask 裁掉；为避免锯齿用纯色填充 + 直角描边。
        painter.fillRect(rect(), m_bgColor);
        painter.setClipping(false);
    }
}
