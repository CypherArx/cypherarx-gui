cmake_minimum_required(VERSION 3.21)
project(cypherarx_gui LANGUAGES CXX VERSION 0.0.1)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

if(MSVC)
  add_compile_options(/utf-8)
endif()

set(CYPHERARX_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(external/cypherarx-core)

find_package(imgui CONFIG REQUIRED)

add_executable(cypherarx_gui
  src/main.cpp
  src/app/app.cpp
  src/app/clipboard_guard.cpp
  src/app/projection.cpp
  src/platform/host_win32_dx11.cpp
  src/platform/imgui_setup.cpp
)
target_include_directories(cypherarx_gui PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(cypherarx_gui PRIVATE
  cypherarx_core
  imgui::imgui
  d3d11
  dxgi
  d3dcompiler
  comdlg32
)

if(MSVC)
  target_compile_options(cypherarx_gui PRIVATE /W4 /permissive-)
endif()
