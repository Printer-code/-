cmake_minimum_required(VERSION 3.16)

project(Qt_test01 VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 【关键】查找 Qt6 模块，必须包含 Multimedia
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Multimedia)

# 列出所有源文件
# 列出所有源文件
set(PROJECT_SOURCES
    src/main.cpp
    src/just_test.cpp
    src/just_test.h
    src/res.qrc  # 注意这里：看你左侧文件树，它叫 res.qrc，且和 CMakeLists.txt 同级
)

# 创建可执行文件
add_executable(${PROJECT_NAME}
    ${PROJECT_SOURCES}
)

# 链接 Qt6 库
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Multimedia # 【关键】必须链接 Multimedia 模块
)

# Windows 下设置子系统（如果是 GUI 程序）
if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()