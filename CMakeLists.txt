cmake_minimum_required(VERSION 3.20)

project(GeoLifeVis
    VERSION 1.0.0
    DESCRIPTION "Real-time KDE heat map visualisation of the Microsoft GeoLife GPS data set using Vulkan compute shaders"
    LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Default to an optimised build for single-config generators (Makefiles, Ninja)
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(GEOLIFE_USE_SYSTEM_DEPS "Prefer GLFW and GLM installed on the system over downloading them" ON)
set(GEOLIFE_DATASET "" CACHE FILEPATH
    "GeoLife data set copied next to the executable (default: data/geoLifeDataSet.txt if present, otherwise the bundled sample)")

# ---------------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------------
include(FetchContent)

# Vulkan (headers + loader) - provided by the Vulkan SDK or the system package manager
find_package(Vulkan REQUIRED)

# GLFW (window + input)
if(GEOLIFE_USE_SYSTEM_DEPS)
    find_package(glfw3 3.3 QUIET)
endif()
if(NOT TARGET glfw)
    message(STATUS "GLFW not found on the system - downloading GLFW 3.4")
    set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG        3.4
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(glfw)
endif()

# GLM (maths, header only)
if(GEOLIFE_USE_SYSTEM_DEPS)
    find_package(glm QUIET)
    if(TARGET glm AND NOT TARGET glm::glm)
        add_library(glm::glm ALIAS glm) # older GLM packages only export "glm"
    endif()
endif()
if(NOT TARGET glm::glm)
    message(STATUS "GLM not found on the system - downloading GLM 1.0.1")
    FetchContent_Declare(glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG        1.0.1
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(glm)
endif()

# Dear ImGui (vendored in third_party/imgui, contains a custom range slider)
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui)
add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/ImGuiRangeSlider.cpp
    ${IMGUI_DIR}/misc/cpp/imgui_stdlib.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp)
target_include_directories(imgui PUBLIC
    ${IMGUI_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
target_link_libraries(imgui PUBLIC glfw Vulkan::Vulkan)

# ---------------------------------------------------------------------------
# Shaders (GLSL -> SPIR-V)
# ---------------------------------------------------------------------------
find_program(GLSLC_EXECUTABLE glslc HINTS "$ENV{VULKAN_SDK}/bin" "$ENV{VULKAN_SDK}/Bin")
find_program(GLSLANG_VALIDATOR_EXECUTABLE glslangValidator HINTS "$ENV{VULKAN_SDK}/bin" "$ENV{VULKAN_SDK}/Bin")
if(NOT GLSLC_EXECUTABLE AND NOT GLSLANG_VALIDATOR_EXECUTABLE)
    message(FATAL_ERROR "Neither glslc nor glslangValidator was found. Install the Vulkan SDK "
                        "or your distribution's glslc / glslang-tools package.")
endif()

set(SHADER_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/shaders)
set(SPIRV_FILES)
foreach(stage vert frag comp)
    set(input  ${CMAKE_CURRENT_SOURCE_DIR}/shaders/shader.${stage})
    set(output ${SHADER_OUTPUT_DIR}/${stage}.spv)
    if(GLSLC_EXECUTABLE)
        set(compile_command ${GLSLC_EXECUTABLE} ${input} -o ${output})
    else()
        set(compile_command ${GLSLANG_VALIDATOR_EXECUTABLE} -V ${input} -o ${output})
    endif()
    add_custom_command(
        OUTPUT  ${output}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_OUTPUT_DIR}
        COMMAND ${compile_command}
        DEPENDS ${input}
        COMMENT "Compiling shader.${stage} -> ${stage}.spv"
        VERBATIM)
    list(APPEND SPIRV_FILES ${output})
endforeach()
add_custom_target(shaders DEPENDS ${SPIRV_FILES})

# ---------------------------------------------------------------------------
# Application
# ---------------------------------------------------------------------------
add_executable(GeoLifeVis
    src/main.cpp
    src/geoLifeData.cpp
    src/geoLifeData.h
    src/ui.h)
target_include_directories(GeoLifeVis PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb)
target_link_libraries(GeoLifeVis PRIVATE imgui glfw glm::glm Vulkan::Vulkan)
add_dependencies(GeoLifeVis shaders)

if(MSVC)
    target_compile_definitions(GeoLifeVis PRIVATE _CRT_SECURE_NO_WARNINGS)
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT GeoLifeVis)
endif()

# Run the program from the directory that contains the shaders, textures and data set
set_target_properties(GeoLifeVis PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:GeoLifeVis>"
    XCODE_SCHEME_WORKING_DIRECTORY "$<TARGET_FILE_DIR:GeoLifeVis>")

# ---------------------------------------------------------------------------
# Runtime files: copy shaders, textures and data set next to the executable
# ---------------------------------------------------------------------------
if(NOT GEOLIFE_DATASET)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/data/geoLifeDataSet.txt)
        set(GEOLIFE_DATASET ${CMAKE_CURRENT_SOURCE_DIR}/data/geoLifeDataSet.txt)
    else()
        set(GEOLIFE_DATASET ${CMAKE_CURRENT_SOURCE_DIR}/data/geoLifeDataSetSample.txt)
    endif()
endif()
message(STATUS "GeoLife data set: ${GEOLIFE_DATASET}")

add_custom_command(TARGET GeoLifeVis POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SPIRV_FILES} "$<TARGET_FILE_DIR:GeoLifeVis>"
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/assets/textures "$<TARGET_FILE_DIR:GeoLifeVis>/textures"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${GEOLIFE_DATASET} "$<TARGET_FILE_DIR:GeoLifeVis>/geoLifeDataSet.txt"
    COMMENT "Copying shaders, textures and data set next to the executable")
