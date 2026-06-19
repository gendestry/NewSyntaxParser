cmake_minimum_required(VERSION 3.10)

# Project setup
project(synparser VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# add_subdirectory(lib/json)
add_subdirectory(lib/Utils)
# add_subdirectory(lib/FTXUI)

# find_package(simpleble CONFIG REQUIRED)

file(GLOB_RECURSE SOURCES src/*.cpp)

add_executable(${PROJECT_NAME} ${SOURCES})

target_link_libraries(${PROJECT_NAME} PRIVATE utils)
target_include_directories(${PROJECT_NAME}
    PUBLIC
    include
    PRIVATE
    ${PROJECT_SOURCE_DIR}/src
)