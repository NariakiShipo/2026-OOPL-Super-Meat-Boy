cmake_minimum_required(VERSION 3.16)
project(SuperMeatBoy)

set(CMAKE_CXX_STANDARD 17)

cmake_policy(SET CMP0135 NEW)

# Some bundled third-party dependencies (e.g. SDL2 2.26, glew) still declare a
# `cmake_minimum_required()` below 3.5, which CMake >= 4.0 rejects outright.
# Provide the documented compatibility floor so they configure on modern CMake.
if(NOT DEFINED CMAKE_POLICY_VERSION_MINIMUM)
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
endif()

include(FetchContent)
set(FETCHCONTENT_QUIET FALSE)

# The PTSD framework is vendored in this repository (see ./PTSD) so that our
# local modifications stay under version control. Build it directly from source
# instead of re-downloading the upstream release (which would overwrite our
# changes). PTSD's own third-party dependencies (SDL2, SDL2_image/ttf/mixer,
# glew, glm, spdlog, ImGui, googletest) are fetched automatically on the first
# configure by PTSD/cmake/Dependencies.cmake.
add_subdirectory(PTSD)

FetchContent_Declare(
    tmxlite

    URL         https://github.com/fallahn/tmxlite/archive/refs/tags/v1.3.1.zip

    SOURCE_DIR  ${CMAKE_CURRENT_SOURCE_DIR}/lib/tmxlite
)

FetchContent_Declare(
    nlohmann_json

    URL         https://github.com/nlohmann/json/archive/refs/tags/v3.11.3.zip
)

FetchContent_MakeAvailable(nlohmann_json)

FetchContent_GetProperties(tmxlite)
if (NOT ${tmxlite_POPULATED})
    FetchContent_Populate(tmxlite)
    add_subdirectory(${tmxlite_SOURCE_DIR}/tmxlite ${tmxlite_BINARY_DIR})
endif()

include(files.cmake)
set(SRC_DIR ${CMAKE_SOURCE_DIR}/src)
set(INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include)
list(TRANSFORM SRC_FILES PREPEND ${SRC_DIR}/)
list(TRANSFORM INCLUDE_FILES PREPEND ${INCLUDE_DIR}/)

# file(GLOB_RECURSE SRC_FILES src/*)
# file(GLOB_RECURSE HEADER_FILES include/*)

add_executable(${PROJECT_NAME}
    ${SRC_DIR}/main.cpp
    ${SRC_FILES}
    ${HEADER_FILES}
)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -pedantic)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(${PROJECT_NAME} PRIVATE RESOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/Resources")
else()
    message(FATAL_ERROR "relative RESOURCE_DIR is WIP, Please use `-DCMAKE_BUILD_TYPE=Debug` build for now.")
    # target_compile_definitions(${PROJECT_NAME} PRIVATE RESOURCE_DIR="${CMAKE_INSTALL_PREFIX}/Resources")
endif()

target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE ${DEPENDENCY_INCLUDE_DIRS})
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/PTSD/include)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(${PROJECT_NAME}
    SDL2::SDL2main
    PTSD
    tmxlite
    nlohmann_json::nlohmann_json
)
