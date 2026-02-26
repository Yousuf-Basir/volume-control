cmake_minimum_required(VERSION 3.10)
project(keypress)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_WIN32_EXECUTABLE ON)

add_executable(keypress WIN32 main.cpp)

if(WIN32)
    set(WINDOWS_SDK_LIB "C:/Program Files (x86)/Windows Kits/10/Lib/10.0.26100.0/um/x64")
    target_link_directories(keypress PRIVATE ${WINDOWS_SDK_LIB})
    target_link_libraries(keypress PRIVATE user32.lib mmdevapi.lib shell32.lib gdiplus.lib)
endif()
