cmake_minimum_required(VERSION 3.13)

project(codecrafters-shell)

file(GLOB_RECURSE SOURCE_FILES src/*.c src/*.h)

set(CMAKE_C_STANDARD 23) # Enable the C23 standard
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(shell ${SOURCE_FILES})

target_include_directories(shell PRIVATE /opt/homebrew/include)
target_link_directories(shell PRIVATE /opt/homebrew/lib)
target_link_libraries(shell PRIVATE readline)
