QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

# 程序信息
VERSION = 2.1.0
DEFINES += APP_VERSION=\\\"$$VERSION\\\"
RC_ICONS = WindowIcon.ico
QMAKE_TARGET_COMPANY = "Explorer"
QMAKE_TARGET_DESCRIPTION = "Gaokao countdown widget for students"
QMAKE_TARGET_PRODUCT = "BattleForFuture"

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    desktopwidget.cpp \
    logger.cpp

HEADERS += \
    mainwindow.h \
    desktopwidget.h \
    logger.h

FORMS += \
    mainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    resource.qrc

TRANSLATIONS += \
    BattleForFuture_zh_CN.ts \
    BattleForFuture_zh_HK.ts

# 开发环境兜底：当运行目录没有 Assets/quotes.txt 时，回退到源码目录的文件
QUOTE_FALLBACK = $$PWD/Assets/quotes.txt
win32: DEFINES += QUOTE_FALLBACK_PATH=\\\"$$quoted($$replace(QUOTE_FALLBACK,/,\\\\))\\\"
else:  DEFINES += QUOTE_FALLBACK_PATH=\\\"$$QUOTE_FALLBACK\\\"
