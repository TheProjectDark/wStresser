cmake_minimum_required(VERSION 4.1)
project(wStresser)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCES
   src/MainFrame.cpp
   src/memLeakTest.cpp
   src/VulkanGraphics/VulkanGraphics.cpp
)

find_package(wxWidgets REQUIRED COMPONENTS core base)
find_package(glfw3 REQUIRED)
find_package(Vulkan REQUIRED)

include(${wxWidgets_USE_FILE})

add_executable(wStresser ${SOURCES})

target_include_directories(wStresser PRIVATE
   ${CMAKE_SOURCE_DIR}/include
)

target_compile_options(wStresser PRIVATE -O2)

target_link_libraries(wStresser PRIVATE
   ${wxWidgets_LIBRARIES}
   glfw
   Vulkan::Vulkan
)