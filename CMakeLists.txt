cmake_minimum_required(VERSION 3.20)

if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
endif()

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

# 1. Автоскачивание MinHook
FetchContent_Declare(
    minhook
    GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git
    GIT_TAG        v1.3.3
)
FetchContent_GetProperties(minhook)
if(NOT minhook_POPULATED)
    FetchContent_Populate(minhook)
endif()

# Собираем MinHook напрямую из исходных файлов (.c)
file(GLOB MINHOOK_SOURCES
    "${minhook_SOURCE_DIR}/src/*.c"
    "${minhook_SOURCE_DIR}/src/HDE/hde32.c"
    "${minhook_SOURCE_DIR}/src/hde/hde32.c"
)

# 2. Автоскачивание Dear ImGui
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.90.5
)
FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
    FetchContent_Populate(imgui)
endif()

set(IMGUI_DIR "${imgui_SOURCE_DIR}")
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_dx9.cpp
    ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
)

include_directories(
    ${minhook_SOURCE_DIR}/include
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

# 3. Поиск main.cpp
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")
    set(MAIN_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")
    set(MAIN_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")
    include_directories(${CMAKE_CURRENT_SOURCE_DIR})
else()
    file(GLOB_RECURSE ALL_MAINS "main.cpp")
    if(ALL_MAINS)
        list(GET ALL_MAINS 0 MAIN_SOURCE)
    else()
        message(FATAL_ERROR "Файл main.cpp не найден!")
    endif()
endif()

# Компилируем main.cpp, ImGui и MinHook в один .asi файл
add_library(${PROJECT_NAME} SHARED
    ${MAIN_SOURCE}
    ${IMGUI_SOURCES}
    ${MINHOOK_SOURCES}
)

# minhook удален из списка, так как скомпилирован напрямую выше
target_link_libraries(${PROJECT_NAME} PRIVATE
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
