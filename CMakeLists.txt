cmake_minimum_required(VERSION 3.10)

# Project setup
project(synparser VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_subdirectory(lib/Utils)

# ---- core parsing library ---------------------------------------------------
# The reusable engine: the tokenizer + the grammar-driven parse engine. It
# knows nothing about any particular language; examples supply their own
# tokens.txt / lang.syn / ast.spec and link against this.
file(GLOB_RECURSE SYNPARSER_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/src/Syntax/*.cpp
    ${CMAKE_SOURCE_DIR}/src/Tokenizer/*.cpp)

add_library(synparser STATIC ${SYNPARSER_SOURCES})
target_link_libraries(synparser PUBLIC utils)
target_include_directories(synparser
        PUBLIC
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
)

# ---- AST generator (shared by examples that declare an ast.spec) ------------
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(AST_GEN_SCRIPT ${CMAKE_SOURCE_DIR}/generator/gen_ast.py)

# add_synparser_example(<name>)
#   Builds an executable from the *.cpp in the calling directory, generates
#   Ast.gen.h from that directory's ast.spec, links the synparser library, and
#   copies the runtime data files next to the binary.
function(add_synparser_example name)
    set(gen_dir ${CMAKE_CURRENT_BINARY_DIR}/gen)
    set(gen_header ${gen_dir}/Ast.gen.h)
    add_custom_command(
        OUTPUT ${gen_header}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${gen_dir}
        COMMAND ${Python3_EXECUTABLE} ${AST_GEN_SCRIPT}
                ${CMAKE_CURRENT_SOURCE_DIR}/ast.spec ${gen_header}
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/ast.spec ${AST_GEN_SCRIPT}
        COMMENT "Generating ${name}/Ast.gen.h from ast.spec")
    add_custom_target(${name}_gen_ast DEPENDS ${gen_header})

    file(GLOB EXAMPLE_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
    add_executable(${name} ${EXAMPLE_SOURCES})
    add_dependencies(${name} ${name}_gen_ast)
    target_link_libraries(${name} PRIVATE synparser)
    target_include_directories(${name} PRIVATE ${gen_dir} ${CMAKE_CURRENT_SOURCE_DIR})

    foreach(DATA_FILE tokens.txt input.txt lang.syn)
        add_custom_command(TARGET ${name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${CMAKE_CURRENT_SOURCE_DIR}/${DATA_FILE}
                $<TARGET_FILE_DIR:${name}>/${DATA_FILE}
            COMMENT "Copying ${DATA_FILE} next to ${name}")
    endforeach()
endfunction()

# ---- examples ---------------------------------------------------------------
add_subdirectory(examples/basic)
add_subdirectory(examples/complex)
add_subdirectory(examples/test)
