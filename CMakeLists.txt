cmake_minimum_required(VERSION 3.25)
project(cypherarx_gui LANGUAGES CXX VERSION 0.0.1)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

option(CYPHERARX_GUI_IMGUI "Build the Dear ImGui front end (cypherarx_gui)" ON)
option(CYPHERARX_GUI_LAMINA "Build the lamina-ui front end (cypherarx_gui_lamina)" ON)

if(MSVC)
  add_compile_options(/utf-8)
endif()

set(CYPHERARX_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(external/cypherarx-core)

if(CYPHERARX_GUI_IMGUI)
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
endif()

if(CYPHERARX_GUI_LAMINA)
  set(LAMINA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(LAMINA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(LAMINA_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
  add_subdirectory(external/lamina-ui)

  add_library(cypherarx_gui_model STATIC
    src/app/projection.cpp
    src/model/session.cpp
    src/model/timed_clipboard.cpp
  )
  target_include_directories(cypherarx_gui_model PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_link_libraries(cypherarx_gui_model PUBLIC cypherarx_core)

  add_executable(cypherarx_gui_lamina
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
  target_link_libraries(cypherarx_gui_lamina PRIVATE
    cypherarx_gui_model
    lamina::lamina
    lamina::thorvg
    lamina::sdl3
    comdlg32
  )
  lamina_add_resources(cypherarx_gui_lamina_resources
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
      folder_open
      grid_view
      light_mode
      lock
      login
      palette
      person
      public
      star
      subdirectory_arrow_right
      timer
      visibility
  )
  target_link_libraries(cypherarx_gui_lamina PRIVATE cypherarx_gui_lamina_resources)

  if(MSVC)
    target_compile_options(cypherarx_gui_model PRIVATE /W4 /permissive-)
    target_compile_options(cypherarx_gui_lamina PRIVATE /W4 /permissive-)
  endif()
endif()
