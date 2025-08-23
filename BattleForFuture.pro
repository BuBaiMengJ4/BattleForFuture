QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

# 设置程序版本
VERSION = 1.0.0

# 设置程序图标
RC_ICONS = WindowIcon.ico

# 设置开发者和app名称
QMAKE_TARGET_COMPANY = "Explorer"
QMAKE_TARGET_DESCRIPTION = "This is a test application"
QMAKE_TARGET_PRODUCT = "BattleForFurture"

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    main.cpp \
    mainwindow.cpp

HEADERS += \
    mainwindow.h

FORMS += \
    mainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    resource.qrc
