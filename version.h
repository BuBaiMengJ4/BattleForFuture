#ifndef APP_VERSION_H
#define APP_VERSION_H

/**
 * @brief 程序版本号
 *
 * 为什么单独放一个头文件，而不是直接用 .pro 里的 DEFINES？
 * qmake 生成的 Makefile **不会**把 -D 宏的变化记为依赖，只改 .pro 里的
 * VERSION 时，已经编译过的 .cpp 不会重新编译，于是“版本号改了但界面还显示旧版本”。
 * 放进头文件后，版本变化会让所有包含它的源文件自动重编，不会再有这个坑。
 *
 * 注意：修改版本号时请同时更新 BattleForFuture.pro 里的 VERSION
 * （那里决定 Windows 可执行文件属性里的版本资源）。
 */
#define APP_VERSION "2.3.5"

#endif // APP_VERSION_H
