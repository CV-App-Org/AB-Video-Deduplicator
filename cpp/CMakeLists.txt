cmake_minimum_required(VERSION 3.16)
project(ab_dedup_cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(Qt5 COMPONENTS Widgets REQUIRED)

add_executable(ab_dedup
    src/main.cpp
    src/MainWindow.cpp
    src/VideoProcessor.cpp
    src/MainWindow.h
    src/VideoProcessor.h
)

target_link_libraries(ab_dedup PRIVATE Qt5::Widgets)
