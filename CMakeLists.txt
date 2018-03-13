cmake_minimum_required(VERSION 3.9)
project(Snake)

add_executable(SnakeGame main.cpp)
target_link_libraries(SnakeGame ncurses)