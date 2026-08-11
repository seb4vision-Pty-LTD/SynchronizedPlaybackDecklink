cmake_minimum_required(VERSION 3.10)
project(DecklinkPlayout)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Find Qt
find_package(QT NAMES Qt6 Qt5 COMPONENTS Widgets REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Widgets REQUIRED)

# Find GStreamer
find_package(PkgConfig REQUIRED)
pkg_check_modules(GST REQUIRED gstreamer-1.0 gstreamer-video-1.0)
include_directories(
    include
)


add_executable(${PROJECT_NAME} src/main.cpp src/MainWindow.cpp include/MainWindow.h)

target_link_directories(${PROJECT_NAME} 
    PRIVATE 
    ${GST_LIBRARY_DIRS}
)

target_include_directories(${PROJECT_NAME} PRIVATE ${GST_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PRIVATE Qt${QT_VERSION_MAJOR}::Widgets ${GST_LIBRARIES})