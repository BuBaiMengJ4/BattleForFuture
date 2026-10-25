#ifndef DIAGNOSTICS_H
#define DIAGNOSTICS_H

#include <QElapsedTimer>
#include <QString>

class QTimer;
class QThread;

/**
 * @brief 诊断设施：崩溃捕获 + 界面卡死检测 + 关键操作埋点
 *
 * 为什么需要它？
 * 之前出现“窗口不显示”“切换设置页卡死”这类问题时，日志里什么都没有，
 * 因为：
 * 1) 程序被未处理的结构化异常（访问违例）直接终止，进程连析构都来不及跑，
 *    自然来不及写任何日志；
 * 2) 界面线程陷入死循环或被阻塞时，日志系统本身也在同一线程上，
 *    同样写不出“我正在卡死”这种信息。
 *
 * 本模块针对这两种情况各给出一条独立通路：
 *
 * - **崩溃**：在 Windows 上注册 SetUnhandledExceptionFilter，
 *   崩溃瞬间用**原生 Win32 API**（不依赖 Qt 的对象/内存分配）把异常代码、
 *   异常地址、出错模块名与偏移写入 Logs 目录，并尝试生成 minidump；
 * - **卡死**：独立监视线程每秒检查一次界面线程的“心跳”。
 *   界面线程用 QTimer 定期刷新心跳；若超过阈值没有刷新，
 *   就把“界面线程疑似卡死”以及最后记录的**操作阶段**写入日志，
 *   直接指出卡死发生在哪个步骤。
 */
namespace Diagnostics {

/// 启动崩溃捕获与卡死监视（应在日志系统初始化之后尽早调用）
void install();

/// 停止监视线程（程序退出时调用）
void shutdown();

/// 界面线程心跳（由内部 QTimer 驱动，通常无需手工调用）
void beat();

/// 记录“当前正在执行的关键操作”。卡死时这条信息会被写进日志，
/// 直接告诉排查者卡在了哪一步。
void setStage(const QString &stage);

/// 当前阶段（供日志附带）
QString currentStage();

/**
 * @brief 作用域式阶段标记：进入时设置阶段，离开时清除
 *
 * 用法：
 * @code
 *   Diagnostics::Step step(QStringLiteral("构建设置页/颜色分组"));
 *   ... 可能出问题的代码 ...
 * @endcode
 */
class Step
{
public:
    explicit Step(const QString &stage);
    ~Step();

    Step(const Step &) = delete;
    Step &operator=(const Step &) = delete;

private:
    QString m_previous;
};

} // namespace Diagnostics

#endif // DIAGNOSTICS_H
