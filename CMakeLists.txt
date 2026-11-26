cmake_minimum_required(VERSION 3.16)

project(zhanglv_expense
    VERSION 0.5.0
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS
    Core Gui Widgets Quick QuickControls2 Sql Test)

qt_standard_project_setup(REQUIRES 6.8)

# 纯 C++ 核心层（账目引擎、金额、SQLite），桌面与 Android 共用，可独立单测
add_subdirectory(src/core)

# 应用可执行目标
qt_add_executable(ZhangLvExpense
    src/app/main.cpp
    src/app/AppController.cpp
    src/app/AppController.h
    src/app/TransactionModel.cpp
    src/app/TransactionModel.h)

qt_add_qml_module(ZhangLvExpense
    URI ZhangLvExpense
    VERSION 1.0
    QML_FILES
        src/app/qml/Main.qml)

target_link_libraries(ZhangLvExpense PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Quick Qt6::QuickControls2 Qt6::Sql
    zlexpense_core)

set_target_properties(ZhangLvExpense PROPERTIES
    QT_ANDROID_PACKAGE_NAME "com.zhanglv.expense"
    QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android"
    QT_ANDROID_VERSION_CODE 5
    QT_ANDROID_VERSION_NAME "0.5.0"
    QT_ANDROID_MIN_SDK_VERSION 26
    QT_ANDROID_TARGET_SDK_VERSION 34)

# 桌面构建：核心层自动化测试（Android 不编译测试）
if(NOT ANDROID)
    enable_testing()
    add_subdirectory(tests)
endif()
