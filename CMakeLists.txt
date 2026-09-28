cmake_minimum_required(VERSION 3.20)
project(GAhelper LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "Плагин компилируется строго под x86 (Win32)!")
endif()

add_compile_definitions(
    WIN32
    _WINDOWS
    _CRT_SECURE_NO_WARNINGS
)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/kiero
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/kiero/minhook/include
)

file(GLOB_RECURSE VENDOR_FILES
    "vendor/imgui/*.cpp"
    "vendor/kiero/*.cpp"
    "vendor/kiero/minhook/src/*.c"
)

add_library(${PROJECT_NAME} SHARED
    src/main.cpp
    ${VENDOR_FILES}
)

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
