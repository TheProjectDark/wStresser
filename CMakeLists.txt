cmake_minimum_required(VERSION 4.1)
project(wStresser)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCES
        src/MainFrame.cpp
        src/memLeakTest.cpp
)

find_package(wxWidgets REQUIRED COMPONENTS core base)

include(${wxWidgets_USE_FILE})

add_executable(wStresser ${SOURCES})

target_compile_options(wStresser PRIVATE -O2)

target_link_libraries(wStresser ${wxWidgets_LIBRARIES})