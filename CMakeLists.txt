cmake_minimum_required(VERSION 3.12)
project(batt-watchdog)

set(CMAKE_CXX_STANDARD 11)

include_directories(/usr/include/SDL2)

add_executable(batt-watchdog src/main.cxx)

target_link_libraries(batt-watchdog SDL2 SDL2_mixer mpg123)
