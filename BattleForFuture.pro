QT       += core gui network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

# 程序信息
# 版本号定义在源码头文件 version.h 中：
# qmake 生成的 Makefile 不把 -D 宏的变化记为依赖，只改这里的 VERSION 时
# 已编译过的文件不会重新编译，会出现“版本号改了但界面还显示旧版本”。
# 这里的 VERSION 仍然保留，用于 Windows 可执行文件属性里的版本资源。
VERSION = 2.3.6
RC_ICONS = WindowIcon.ico
QMAKE_TARGET_COMPANY = "Explorer"
QMAKE_TARGET_DESCRIPTION = "Gaokao countdown widget for students"
QMAKE_TARGET_PRODUCT = "BattleForFuture"

CONFIG += c++17

# 说明：小组件样式 2（右下角透明区域）改为纯 QWidget + QPainter 实现，
# 不再需要 Qt Quick/QML 运行时，因此这里移除了 quick/qml/quickwidgets，
# 可执行文件更小、启动更快，也少了一个渲染后端带来的平台差异。
# 若将来需要 QML，请在这里重新加上：QT += quick qml quickwidgets

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    main.cpp \
    appinstance.cpp \
    diagnostics.cpp \
    mainwindow.cpp \
    settingsdialog.cpp \
    desktopwidget.cpp \
    overlaywidget.cpp \
    sevensegmentdisplay.cpp \
    scrollinglabel.cpp \
    slidingnavlist.cpp \
    textutils.cpp \
    widgetstyle.cpp \
    iconutils.cpp \
    platformsupport.cpp \
    restarthelper.cpp \
    notifier.cpp \
    logger.cpp

HEADERS += \
    mainwindow.h \
    appinstance.h \
    diagnostics.h \
    settingsdialog.h \
    desktopwidget.h \
    overlaywidget.h \
    sevensegmentdisplay.h \
    scrollinglabel.h \
    slidingnavlist.h \
    textutils.h \
    version.h \
    restarthelper.h \
    notifier.h \
    widgetstyle.h \
    iconutils.h \
    platformsupport.h \
    logger.h

FORMS += \
    mainwindow.ui

# Windows 崩溃诊断需要 dbghelp（MiniDumpWriteDump）
win32: LIBS += -ldbghelp

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    resource.qrc \
    translations.qrc

# 翻译策略说明
# ---------------------------------------------------------------------------
# 本程序源码里的界面文案就是简体中文，繁体界面由 TextUtils::toTraditional()
# 在运行时做简 -> 繁转换（见 textutils.h），因此**不需要**维护两份 .ts，
# 也不会出现“新增文案忘了补繁体”的漏翻。
# qt_zh_CN.qm（Qt 官方中文翻译，负责颜色对话框等标准控件）已随 resource.qrc
# 一并打进可执行文件。
#
# 保留 TRANSLATIONS 只是为了在需要时用 Qt Linguist 导出字符串清单：
#     lupdate BattleForFuture.pro
# 注意不要打开 lrelease 自动构建，否则会生成只有 26 字节的空 .qm 被误当成翻译。
# TRANSLATIONS += \
#     BattleForFuture_zh_CN.ts \
#     BattleForFuture_zh_HK.ts

# 开发环境兜底：当运行目录没有 Assets/quotes.txt 时，回退到源码目录的文件
QUOTE_FALLBACK = $$PWD/Assets/quotes.txt
win32: DEFINES += QUOTE_FALLBACK_PATH=\\\"$$quoted($$replace(QUOTE_FALLBACK,/,\\\\))\\\"
else:  DEFINES += QUOTE_FALLBACK_PATH=\\\"$$QUOTE_FALLBACK\\\"

