cmake_minimum_required(VERSION 3.25)
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

set(LAMINA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LAMINA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LAMINA_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
add_subdirectory(external/lamina-ui)

add_library(cypherarx_gui_model STATIC
  src/model/projection.cpp
  src/model/session.cpp
  src/model/timed_clipboard.cpp
)
target_include_directories(cypherarx_gui_model PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(cypherarx_gui_model PUBLIC cypherarx_core)

lamina_add_resources(cypherarx_gui_resources
  NAMESPACE cypherarx::lamina_gui::res
  HEADER lamina_gui/res.hpp
  ICONS
    account_circle
    add
    category
    check
    chevron_right
    content_copy
    create_new_folder
    dark_mode
    delete
    edit
    expand_more
    folder
    folder_open
    grid_view
    inventory_2
    light_mode
    lock
    login
    palette
    person
    public
    sell
    star
    subdirectory_arrow_right
    timer
    visibility
)

add_executable(cypherarx_gui
  src/lamina_gui/main.cpp
  src/lamina_gui/app.cpp
  src/lamina_gui/controls.cpp
  src/lamina_gui/list_view.cpp
  src/lamina_gui/secret_field.cpp
  src/lamina_gui/select_field.cpp
  src/lamina_gui/sheet.cpp
  src/lamina_gui/sheets.cpp
  src/platform/file_dialog_win32.cpp
)
target_link_libraries(cypherarx_gui PRIVATE
  cypherarx_gui_model
  cypherarx_gui_resources
  lamina::lamina
  lamina::thorvg
  lamina::sdl3
  comdlg32
)

if(MSVC)
  target_compile_options(cypherarx_gui_model PRIVATE /W4 /permissive-)
  target_compile_options(cypherarx_gui PRIVATE /W4 /permissive-)
endif()