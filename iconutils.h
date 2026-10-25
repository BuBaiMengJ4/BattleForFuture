#ifndef ICONUTILS_H
#define ICONUTILS_H

#include <QIcon>
#include <QString>

/**
 * @brief 图标处理工具
 *
 * 解决的问题：按钮图标看起来“偏向一角”。
 *
 * 原因不在按钮布局，而在**图片本身**：Qt 会把 QIcon 按图片的几何中心居中到按钮上，
 * 如果图片内容（不透明像素）本身偏在画布的一侧，视觉上就会偏心。
 * 实测本项目两张图：
 *   - Settings.png 316×265，内容质心比图片中心高 12.4px、右 1.6px；
 *   - Widget.png   130×123，内容质心比图片中心左 7.9px。
 * 因此左键图标偏左上、右键图标偏左，与按钮尺寸无关。
 *
 * 处理方式：裁剪到不透明内容的包围盒，再居中放进正方形画布。
 * 这样图标在任意尺寸下都真正居中，而且不会被拉伸变形。
 */
namespace IconUtils {

/**
 * @brief 把资源里的图标规整为“内容居中”的正方形图标
 * @param resourcePath 形如 ":/icon/Settings.png"
 * @param size         输出画布边长（像素）
 * @return 规整后的图标；加载失败时返回空 QIcon
 */
QIcon centeredIcon(const QString &resourcePath, int size = 128);

} // namespace IconUtils

#endif // ICONUTILS_H
