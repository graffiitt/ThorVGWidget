cmake_minimum_required(VERSION 3.16)
project(thorvg_widget)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 COMPONENTS Widgets REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(THORVG REQUIRED IMPORTED_TARGET thorvg-1)

add_executable(
    ${PROJECT_NAME} MACOSX_BUNDLE
    main.cpp
    DemoWindow.cpp
    DemoWindow.h
    ThorVGWidget.cpp
    ThorVGWidget.h
    )
 
target_link_libraries(${PROJECT_NAME} ${catkin_LIBRARIES} Qt6::Widgets PkgConfig::THORVG)
target_include_directories(${PROJECT_NAME} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
  
set(MACOSX_BUNDLE_GUI_IDENTIFIER "your.bundle.identifier")
