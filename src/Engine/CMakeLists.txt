cmake_minimum_required(VERSION 3.16)
project(fem_engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(fem_engine
    Element.cpp
    FileReader.cpp
    Main.cpp
    Material.cpp
    Model.cpp
    Solver.cpp
)

target_include_directories(fem_engine PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/External"
)

if(MSVC)
    target_compile_options(fem_engine PRIVATE /W3 /permissive-)
else()
    target_compile_options(fem_engine PRIVATE -Wall -Wextra)
endif()

set_target_properties(fem_engine PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/bin"
)
