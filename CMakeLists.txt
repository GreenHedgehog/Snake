cmake_minimum_required(VERSION 3.9)
project(Snake)

set(CMAKE_CXX_STANDARD 17)

add_executable(SnakeGame main.cpp)
target_link_libraries(SnakeGame ncurses)