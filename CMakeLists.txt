cmake_minimum_required(VERSION 3.15)
project(vr-launcher LANGUAGES CXX)

add_definitions(-DUNICODE -D_UNICODE)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "Este proyecto debe compilarse como x86 (32-bit). Por favor, usa el generador '-A Win32'.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(vr-launcher WIN32
    src/obfuscate.h
    src/main.cpp
    src/main.h
    src/utils.cpp
    src/utils.h
    src/injector.cpp
    src/injector.h
    src/downloader.cpp
    src/downloader.h
    src/config.cpp
    src/config.h
    src/resources.rc
    src/resource.h
)

set(WEBVIEW2_SDK_PATH ${CMAKE_CURRENT_SOURCE_DIR}/third-party/webview2)
target_include_directories(vr-launcher PRIVATE "${WEBVIEW2_SDK_PATH}/include")

target_link_libraries(vr-launcher PRIVATE
    "${WEBVIEW2_SDK_PATH}/lib/x86/WebView2LoaderStatic.lib"
    ole32
    user32
    gdi32
)

message(STATUS "Proyecto configurado para compilar un .exe autónomo en x86.")