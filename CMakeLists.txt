cmake_minimum_required(VERSION 3.21)

project(bot
    VERSION 0.1
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(ENGINE_VERSION "v0.13.3_eval")

add_library(mondfisch 
    src/game.cpp
    src/evaluation.cpp
    src/engine_search.cpp
    src/uci.cpp
)
target_include_directories(mondfisch 
    PUBLIC include
    PRIVATE src
)

add_executable(${ENGINE_VERSION} src/engine.cpp)
target_link_libraries(${ENGINE_VERSION} PRIVATE mondfisch)
set_target_properties(${ENGINE_VERSION} 
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/engines/${ENGINE_VERSION}
)
add_custom_command(TARGET ${ENGINE_VERSION}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy 
    $<TARGET_FILE:${ENGINE_VERSION}> 
    ${CMAKE_BINARY_DIR}/engines/latest_engine
    COMMENT "Updating symlink"
)


find_package(Catch2 3 REQUIRED)
add_executable(tests tests/test.cpp)
target_link_libraries(tests PRIVATE Catch2::Catch2WithMain)
target_link_libraries(tests PRIVATE mondfisch)

target_compile_options(${ENGINE_VERSION} PRIVATE
    -Wall -Wextra -Wpedantic
)

add_custom_target(run
    COMMAND testing
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

