#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include "widgetstyle.h"

#include <QColor>
#include <QDialog>
#include <QHash>
#include <QString>

class QListWidget;
class QPushButton;
class QStackedWidget;
class QComboBox;
class QCheckBox;
class QLabel;
class QScrollArea;
class QSpinBox;
class QTextEdit;
class QVariant;
class QWheelEvent;
class SlidingNavList;

/**
 * @brief 应用设置对话框（分类导航 + 右侧内容区）
 *
 * 设计要点（对应“优化分类管理和布局逻辑、显示逻辑和操作显示”）：
 * - 分类集中定义在 kCategories 表里，列表项与页面一一对应，新增分类只改一处；
 * - 页面**首次访问时才构建**（惰性加载），打开设置对话框不再一次性创建全部控件，
 *   启动更快、内存占用更低；构建后缓存复用，来回切换零开销；
 * - 页面切换动画轻量（160ms 位移动画），不再每次切换都新建两个动画对象；
 * - 所有配置项都在“当前值 → 用户修改 → 立即写 config.ini → 通知主窗口”这一条链路上，
 *   不会出现界面显示与配置文件不一致的情况；
 * - 小组件样式 2 没有背景色概念：选中后“背景色”按钮自动禁用并给出原因提示，
 *   但保留用户原来的背景色取值，切回样式 1 时立即恢复。
 */
class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(const QString &configPath, const QString &quotesFilePath,
                            QWidget *parent = nullptr);

signals:
    /// 小组件配置发生变化（主窗口据此重建正在运行的桌面小组件）
    void widgetConfigChanged(const WidgetConfig &config);
    /// 高考年份变化（主窗口据此刷新倒计时目标日期）
    void targetYearChanged(int year);
    /// 鼓励语句文件已被保存（主窗口据此刷新首页语句缓存）
    void quotesChanged();

protected:
    /**
     * @brief 把滚轮事件转发给设置页里的滚动区域
     *
     * Qt 默认不会把“落在子控件上的滚轮事件”冒泡给父级 QScrollArea，
     * 所以用户在小组件设置页上滚动滚轮时界面毫无反应。
     * 这里统一拦截并转发；顺带避免了“滚轮停在下拉框上就把样式/颜色改掉”
     * 这种容易误操作的行为。
     */
    void wheelEvent(QWheelEvent *event) override;

private:
    // ---------------- 分类 / 页面 ----------------
    QWidget *createGeneralPage();
    QWidget *createWidgetPage();
    QWidget *createWordEditPage();
    QWidget *createAboutPage();

    /// 返回指定分类的页面，首次调用时构建
    QWidget *pageAt(int index);

    // ---------------- 布局小工具 ----------------
    QLabel  *makePageTitle(const QString &text);
    QLabel  *makeHint(const QString &text);
    QPushButton *makeColorButton(const QString &objectName);
    void     paintColorButton(QPushButton *button, const QString &label, const QColor &color);

    // ---------------- 配置读写 ----------------
    /// 读取 ini（带异常保护），key 形如 "Widget/Style"
    QString readValue(const QString &key, const QString &fallback = QString()) const;
    void    writeValue(const QString &key, const QVariant &value);
    int     readAcademyYear() const;

    // ---------------- 交互逻辑 ----------------
    void showPage(int index, bool animate);
    /**
     * @brief 用左右滑动动画把内容区从 from 切到 to
     * @param from 当前页面（可为空，表示首次显示，不做动画）
     * @param to   目标页面
     * @param direction 1 = 目标在右侧（向左滑入），-1 = 目标在左侧
     *
     * 实现方式：把两页各截一张图，用两个 QLabel 盖在内容区上做位移动画，
     * 动画结束再删掉。**不碰任何布局、不用 QGraphicsEffect** ——
     * 之前用 QGraphicsOpacityEffect 做整页特效时触发过访问违例（0xC0000005），
     * 这条路径（纯 QLabel + 位置动画）不会进入离屏渲染的复杂分支，安全得多。
     */
    void animatePageSwitch(QWidget *from, QWidget *to, int direction);
    /// 清理上一轮滑动动画留下的临时控件与动画对象
    void cleanupPageSlide();
    void applyColorButtonClick(QPushButton *button, const QString &colorKey, const QString &label);
    /// 样式 2 时禁用背景色设置入口
    void updateBackgroundAvailability(int style);
    /// 读取当前小组件配置并广播出去
    void emitWidgetConfig();
    /// 开机自启动（Windows 注册表 / Linux autostart desktop 文件）
    bool autoStartEnabled() const;
    void applyAutoStart(bool enabled);
    /**
     * @brief 以当前配置重新启动本程序
     *
     * 翻译器只在启动时装载，因此语言切换后必须重启才能完整生效。
     * 实现方式：先藏起所有窗口，再用 QProcess::startDetached 拉起新进程，
     * 最后退出当前进程 —— QProcess 在 Windows 上会走 CreateProcess，
     * 对 GUI 子系统程序是安全的（不会弹出多余的控制台窗口）。
     */
    void restartApplication();

    // ---------------- 成员 ----------------
    QString m_configPath;
    QString m_quotesFilePath;

    SlidingNavList *m_categoryList = nullptr;
    QStackedWidget *m_pages        = nullptr;
    /// 分类索引 -> 页面指针（页面按需构建，加入 QStackedWidget 的顺序与分类顺序无关）
    QHash<int, QWidget *> m_pageCache;
    /// 小组件设置页的滚动区域（滚轮事件转发目标）
    QScrollArea    *m_scrollArea   = nullptr;
    /// 当前分类（用于判断左右滑动方向）
    int             m_currentCategory = 0;
    /// 页面滑动动画期间覆盖在内容区上的两个临时图像标签
    QLabel         *m_slideLabelFrom = nullptr;
    QLabel         *m_slideLabelTo   = nullptr;

    // 常规设置页控件
    QCheckBox *m_autoStartCheck = nullptr;
    QComboBox *m_startupModeCombo = nullptr;
    QComboBox *m_languageCombo = nullptr;
    QComboBox *m_yearCombo = nullptr;

    // 小组件页面控件
    QComboBox   *m_styleCombo = nullptr;
    QLabel      *m_styleHint = nullptr;
    QComboBox   *m_targetCombo = nullptr;
    QCheckBox   *m_alwaysOnTopCheck = nullptr;
    QSpinBox    *m_fontSizeSpin = nullptr;
    QPushButton *m_textColorBtn = nullptr;
    QPushButton *m_lcdColorBtn = nullptr;
    QPushButton *m_bgColorBtn = nullptr;
    QLabel      *m_bgColorHint = nullptr;

    // 文字编辑页控件
    QTextEdit *m_quotesEdit = nullptr;
    QPushButton *m_saveQuotesBtn = nullptr;
    QCheckBox *m_noticeCheck = nullptr;
};

#endif // SETTINGSDIALOG_H
