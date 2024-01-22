cmake_minimum_required(VERSION 3.12)

add_executable(test_empty test_empty.cpp)
target_link_libraries(test_empty PUBLIC libearley)