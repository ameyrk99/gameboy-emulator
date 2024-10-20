cmake_minimum_required(VERSION 3.5.0)
project(gameboy VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Use the package PkgConfig to detect GTK+ headers/library files
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTKMM REQUIRED gtkmm-3.0)

add_executable(gameboy main.cpp Z80.cpp)

# Add other flags to the compiler
target_compile_options(gameboy PRIVATE ${GTKMM_CFLAGS_OTHER})
target_link_libraries(gameboy PRIVATE ${GTKMM_LIBRARIES})

# Setup CMake to use GTK+, tell the compiler where to look for headers
# and to the linker where to look for libraries
target_include_directories(gameboy PRIVATE ${GTKMM_INCLUDE_DIRS})
target_link_directories(gameboy PRIVATE ${GTKMM_LIBRARY_DIRS})

# For VSCode
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include") 
add_definitions(-DCMAKE_EXPORT_COMPILE_COMMANDS=ON)
