QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

# 程序信息
VERSION = 2.0.0
RC_ICONS = WindowIcon.ico
QMAKE_TARGET_COMPANY = "Explorer"
QMAKE_TARGET_PRODUCT = "BattleForFurture"
QMAKE_TARGET_DESCRIPTION = "BattleForFurture"

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

TRANSLATIONS += \
    BattleForFuture_zh_HK.ts

update_translations.target = translations
update_translations.commands = lupdate -no-obsolete $$PWD/your_project.pro && lupdate $$PWD/your_project.pro
update_translations.depends = FORCE

QMAKE_EXTRA_TARGETS += update_translations

