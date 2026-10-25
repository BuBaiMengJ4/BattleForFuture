#ifndef TEXTUTILS_H
#define TEXTUTILS_H

#include <QString>

/**
 * @brief 文本转换工具
 *
 * 为什么需要简体 -> 繁体转换？
 * 本项目源码里的界面文案全部是简体中文（`tr("常规设置")` 这类）。
 * Qt 的翻译机制按“上下文 + 源字符串”精确匹配 `.qm` 里的条目，
 * 因此如果要出繁体界面，就必须把上百条文案逐条翻成繁体并维护两份；
 * 一旦新增文案忘了补繁体，界面上就会出现简繁混排。
 *
 * 这里改为在**运行时做字符级转换**：
 * 翻译器加载后，用 QTranslator 的子类把输出的简体文本统一转成繁体。
 * 好处是新增文案自动就有繁体版本，不会漏翻；
 * 代价是个别“一词多译”的词组无法自动处理（例如「设置」在繁体里
 * 通常写作「設定」而不是逐字转换的「設置」），所以额外加了一张词组表
 * 处理这类高频词，转换时先做词组替换、再做逐字转换。
 */
namespace TextUtils {

/// 简体转繁体：先按词组表处理常见词汇，再逐字转换
QString toTraditional(const QString &simplified);

} // namespace TextUtils

#endif // TEXTUTILS_H
