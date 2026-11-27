cmake_minimum_required(VERSION 3.20)
project(OmnilynxEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Target Windows 10/11 (Home & Pro) with full Unicode
add_compile_definitions(
    _WIN32_WINNT=0x0A00
    WINVER=0x0A00
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

if(MSVC)
    # Static Runtime Linking (/MT) for Release builds: zero VCRUNTIME140/MSVCP140 DLL dependencies
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    add_compile_options(/W4 /WX /utf-8 /EHsc)
else()
    # LLVM-MinGW / Clang static linking
    add_compile_options(-Wall -Wextra -Wpedantic -Werror -O2)
    add_link_options(-static -static-libgcc -static-libstdc++ -municode)
endif()

add_executable(wendo_injector
    src/wendo_injector.cpp
    src/resource.rc
)

target_include_directories(wendo_injector PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(wendo_injector PRIVATE
    kernel32
    user32
    shell32
    dwmapi
    psapi
    ole32
    uuid
    winhttp
    bcrypt
    advapi32
)

set_target_properties(wendo_injector PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin"
)
