#include "iconutils.h"

#include "logger.h"

#include <QImage>
#include <QPainter>
#include <QPixmap>

namespace {

/// 小于这个 alpha 值的像素视为透明（避免抗锯齿边缘把包围盒撑大）
constexpr int kAlphaThreshold = 8;

/// 求不透明内容的包围盒；全透明时返回空矩形
QRect contentBounds(const QImage &image)
{
    int minX = image.width(), maxX = -1, minY = image.height(), maxY = -1;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (image.pixelColor(x, y).alpha() > kAlphaThreshold) {
                minX = qMin(minX, x);
                maxX = qMax(maxX, x);
                minY = qMin(minY, y);
                maxY = qMax(maxY, y);
            }
        }
    }
    if (maxX < minX || maxY < minY)
        return QRect();
    return QRect(minX, minY, maxX - minX + 1, maxY - minY + 1);
}

} // namespace

QIcon IconUtils::centeredIcon(const QString &resourcePath, int size)
{
    if (size <= 0)
        size = 128;

    QImage source(resourcePath);
    if (source.isNull()) {
        Logger::warning(QStringLiteral("图标加载失败: %1").arg(resourcePath));
        return QIcon();
    }

    // 统一成带 Alpha 的格式，后续裁剪/绘制都以此为准
    if (source.format() != QImage::Format_ARGB32
        && source.format() != QImage::Format_ARGB32_Premultiplied) {
        source = source.convertToFormat(QImage::Format_ARGB32);
    }

    const QRect bounds = contentBounds(source);
    if (bounds.isEmpty()) {
        Logger::warning(QStringLiteral("图标内容全透明: %1").arg(resourcePath));
        return QIcon(QPixmap::fromImage(source));
    }

    // 以内容包围盒为源，等比缩放到正方形画布内并居中绘制：
    // 这样图标内容在任何控件尺寸下都位于正中心，且保持原比例不变形。
    const QImage cropped = source.copy(bounds);
    const QImage scaled = cropped.scaled(size, size, Qt::KeepAspectRatio,
                                         Qt::SmoothTransformation);

    QImage canvas(size, size, QImage::Format_ARGB32_Premultiplied);
    canvas.fill(Qt::transparent);
    {
        QPainter painter(&canvas);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        painter.drawImage((size - scaled.width()) / 2,
                          (size - scaled.height()) / 2,
                          scaled);
    }

    Logger::debug(QStringLiteral("图标已规整居中: %1（内容 %2x%3 @ %4,%5 -> %6x%6 画布）")
                      .arg(resourcePath)
                      .arg(bounds.width()).arg(bounds.height())
                      .arg(bounds.x()).arg(bounds.y())
                      .arg(size));
    return QIcon(QPixmap::fromImage(canvas));
}
