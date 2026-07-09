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

# ---- generated AST header (gen_ast.py reads ast.spec) -----------------------
# Regenerate build/gen/Ast.gen.h whenever the spec or the generator changes.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(AST_SPEC ${CMAKE_SOURCE_DIR}/ast.spec)
set(AST_GEN_DIR ${CMAKE_BINARY_DIR}/gen)
set(AST_GEN_HEADER ${AST_GEN_DIR}/Ast.gen.h)
add_custom_command(
    OUTPUT ${AST_GEN_HEADER}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${AST_GEN_DIR}
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/gen_ast.py ${AST_SPEC} ${AST_GEN_HEADER}
    DEPENDS ${AST_SPEC} ${CMAKE_SOURCE_DIR}/gen_ast.py
    COMMENT "Generating Ast.gen.h from ast.spec")
add_custom_target(gen_ast DEPENDS ${AST_GEN_HEADER})
add_dependencies(${PROJECT_NAME} gen_ast)

target_link_libraries(${PROJECT_NAME} PRIVATE utils)
target_include_directories(${PROJECT_NAME}
    PUBLIC
    include
    PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${AST_GEN_DIR}
)

# Copy the runtime data files next to the binary so the program finds them
# when run from the build directory. Re-run on every build in case they change.
foreach(DATA_FILE tokens.txt input.txt lang.syn)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/${DATA_FILE}
            $<TARGET_FILE_DIR:${PROJECT_NAME}>/${DATA_FILE}
        COMMENT "Copying ${DATA_FILE} to build output")
endforeach()
