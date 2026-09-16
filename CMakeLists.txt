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

find_program(GLSLC_EXECUTABLE glslc
        HINTS
        $ENV{VULKAN_SDK}/Bin
        $ENV{VULKAN_SDK}/bin
        ${Vulkan_GLSLC_EXECUTABLE}
)

if(NOT GLSLC_EXECUTABLE)
    message(FATAL_ERROR "glslc not found. Install Vulkan SDK and make sure glslc is in PATH.")
endif()

set(SHADER_SRC_DIR ${CMAKE_SOURCE_DIR}/shaders)
set(SHADER_OUT_DIR ${CMAKE_BINARY_DIR}/shaders)

file(MAKE_DIRECTORY ${SHADER_OUT_DIR})

set(SHADER_SOURCES
        ${SHADER_SRC_DIR}/cube.vert
        ${SHADER_SRC_DIR}/cube.frag
)

set(SHADER_SPVS "")
foreach(SHADER ${SHADER_SOURCES})
    get_filename_component(SHADER_NAME ${SHADER} NAME)
    set(SPV ${SHADER_OUT_DIR}/${SHADER_NAME}.spv)
    list(APPEND SHADER_SPVS ${SPV})

    add_custom_command(
            OUTPUT  ${SPV}
            COMMAND ${GLSLC_EXECUTABLE} ${SHADER} -o ${SPV}
            DEPENDS ${SHADER}
            COMMENT "Compiling shader: ${SHADER_NAME} -> ${SPV}"
            VERBATIM
    )
endforeach()

add_custom_target(CompileShaders ALL DEPENDS ${SHADER_SPVS})
add_dependencies(wStresser CompileShaders)

add_custom_command(TARGET wStresser POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:wStresser>/shaders
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${SHADER_OUT_DIR}
        $<TARGET_FILE_DIR:wStresser>/shaders
        COMMENT "Copying .spv next to executable"
)