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

#include <cmath>

DesktopWidget::DesktopWidget(const QColor &backgroundColor,
                             bool showYiMo,
                             const QDate &gaokaoDate,
                             const QDate &yimoDate,
                             QWidget *parent)
    : QDialog(parent)
{
    // 无边框 + Tool：不占任务栏，可当桌面小组件
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool);
    // 逐像素 Alpha，配合遮罩实现真圆角（四角完全透明，无阴影残留）
    setAttribute(Qt::WA_TranslucentBackground);
    setObjectName(QStringLiteral("DesktopWidget"));
    setFixedSize(250, 200);
    setCursor(Qt::SizeAllCursor); // 提示整体可拖动

    // ---- 倒计时内容 ----
    QFont titleFont(QStringLiteral("宋体"), 20);

    m_titleLabel = new QLabel(tr("还有"), this);
    m_titleLabel->setFont(titleFont);
    m_titleLabel->setGeometry(10, 0, 80, 30);

    m_unitLabel = new QLabel(this);
    m_unitLabel->setFont(titleFont);
    m_unitLabel->setGeometry(70, 140, 160, 30);

    m_lcd = new QLCDNumber(this);
    m_lcd->move(30, 30);
    m_lcd->setDigitCount(3);
    m_lcd->setSegmentStyle(QLCDNumber::Flat);
    m_lcd->setFrameShape(QFrame::NoFrame);
    m_lcd->resize(200, 100);
    m_lcd->setStyleSheet(QStringLiteral("color: rgb(255, 0, 0); background: transparent;"));

    if (showYiMo && yimoDate.isValid()) {
        m_lcd->display(remainingDays(yimoDate));
        m_unitLabel->setText(tr("天到一模"));
    } else {
        m_lcd->display(remainingDays(gaokaoDate));
        m_unitLabel->setText(tr("天到高考"));
    }

    // ---- 按钮 ----
    m_exitBtn = new QPushButton(tr("退出"), this);
    m_exitBtn->setGeometry(10, 179, 50, 20);
    m_returnBtn = new QPushButton(tr("返回主界面"), this);
    m_returnBtn->setGeometry(140, 179, 100, 20);

    connect(m_exitBtn, &QPushButton::clicked, qApp, &QApplication::quit);
    connect(m_returnBtn, &QPushButton::clicked, this, &DesktopWidget::returnToMainRequested);

    // ---- 背景色（画在 paintEvent 中，样式表对顶层无边框窗口的 border-radius 无效）----
    m_bgColor = backgroundColor.isValid() ? backgroundColor : QColor(255, 0, 0);

    // 初始位置：屏幕右上角（保持原有习惯，之后可用鼠标拖到任意位置）
    if (QScreen *desktop = QApplication::primaryScreen()) {
        const QRect avail = desktop->availableGeometry();
        move(avail.right() - width(), avail.top());
    }

    Logger::info(QStringLiteral("桌面小组件已创建（可拖拽移动，圆角半径 %1）").arg(m_radius));
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
    rebuildMask();
    QDialog::resizeEvent(event);
}

void DesktopWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    // 直接绘制带抗锯齿的圆角矩形背景；四角因遮罩而真正透明（无阴影）
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QPen borderPen(QColor(224, 208, 160), 1); // 保留原 1px 描边
    borderPen.setCapStyle(Qt::RoundCap);
    painter.setPen(borderPen);

    QPainterPath path;
    path.addRoundedRect(rect().adjusted(0, 0, -1, -1), m_radius, m_radius);

    painter.fillPath(path, m_bgColor);          // 圆角背景色
    painter.drawPath(path);                     // 描边
}
