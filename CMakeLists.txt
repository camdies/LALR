cmake_minimum_required(VERSION 3.16)
project(LALR VERSION 1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)
qt_add_executable(LALR
    src/main.cpp
    src/mainwindow.h
    src/mainwindow.cpp
    src/mainwindow.ui
    src/detailwindow.h
    src/detailwindow.cpp
    src/detailwindow.ui
    src/lalr.h
    src/lalr.cpp
)
target_include_directories(LALR PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(LALR PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)
set_target_properties(LALR PROPERTIES
    WIN32_EXECUTABLE TRUE
)