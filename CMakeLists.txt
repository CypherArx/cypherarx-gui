cmake_minimum_required(VERSION 3.21)
project(cypherarx_gui LANGUAGES CXX VERSION 0.0.1)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Build cypherarx-core (Qt-free) with the same toolchain as the GUI; no tests here.
set(CYPHERARX_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(external/cypherarx-core)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(cypherarx_gui
  src/main.cpp
)
target_link_libraries(cypherarx_gui PRIVATE cypherarx_core Qt6::Widgets)
