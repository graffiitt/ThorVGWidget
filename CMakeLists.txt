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

file(GLOB CPP_FILE "*.cpp")
message("Adding ${PROJECT_NAME} CPP files: " )
foreach(source ${CPP_FILE})
    message("  - " ${source})
endforeach(source)
 
file(GLOB H_FILE "*.h")
message("Adding ${PROJECT_NAME} H files: " )
foreach(source ${H_FILE})
    message("  - " ${source})
endforeach(source)

file(GLOB UI_FILE "*.ui")
message("Adding ${PROJECT_NAME} UI files: " )
foreach(source ${UI_FILE})
    message("  - " ${source})
endforeach(source)


add_executable(
    ${PROJECT_NAME} MACOSX_BUNDLE
    ${CPP_FILE}
    ${H_FILE}
    ${UI_FILE}
    )
 
target_link_libraries(${PROJECT_NAME} ${catkin_LIBRARIES} Qt6::Widgets PkgConfig::THORVG)
target_include_directories(${PROJECT_NAME} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
  
