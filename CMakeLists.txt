cmake_minimum_required(VERSION 3.16)
project(AlgorithmVisualizer VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Qt 安装路径（适配 E:\QT5\qt5 安装）
set(Qt6_DIR "E:/QT5/qt5/lib/cmake/Qt6")
set(CMAKE_PREFIX_PATH "E:/QT5/qt5;${CMAKE_PREFIX_PATH}")

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui)

# 自动收集源文件
file(GLOB_RECURSE SOURCES "src/*.cpp")
file(GLOB_RECURSE HEADERS "src/*.h")

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

target_include_directories(${PROJECT_NAME} PRIVATE src)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Gui
)

# 复制 Qt 运行时 DLL 到输出目录（可选，方便分发）
if(WIN32)
    find_program(QT_QMAKE_EXECUTABLE qmake PATHS "E:/QT5/qt5/bin" NO_DEFAULT_PATH)
    if(QT_QMAKE_EXECUTABLE)
        execute_process(
            COMMAND ${QT_QMAKE_EXECUTABLE} -query QT_INSTALL_PREFIX
            OUTPUT_VARIABLE QT_INSTALL_PREFIX
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        message(STATUS "Qt6 prefix: ${QT_INSTALL_PREFIX}")
    endif()
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
