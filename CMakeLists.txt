cmake_minimum_required(VERSION 3.20)
project(GAhelper LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "Компиляция возможна только под x86 (Win32)!")
endif()

add_compile_definitions(
    WIN32
    _WINDOWS
    _CRT_SECURE_NO_WARNINGS
)

include(FetchContent)


FetchContent_Declare(
    minhook
    GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git
    GIT_TAG        v1.3.3
)
FetchContent_MakeAvailable(minhook)


FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.90.5
)
FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
    FetchContent_Populate(imgui)
    set(IMGUI_DIR ${imgui_SOURCE_DIR})
    set(IMGUI_SOURCES
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/backends/imgui_impl_dx9.cpp
        ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
    )
endif()

include_directories(
    src
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

add_library(${PROJECT_NAME} SHARED
    src/main.cpp
    ${IMGUI_SOURCES}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    minhook
    d3d9
    shell32
    user32
    gdi32
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    SUFFIX ".asi"
    OUTPUT_NAME "GAhelper"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
)
