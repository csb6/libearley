cmake_minimum_required(VERSION 3.12)

add_library(big_array STATIC big_array.cpp)
target_include_directories(big_array PUBLIC .)

add_library(libearley INTERFACE earley.hpp earley_print.hpp)
target_link_libraries(libearley INTERFACE Boost::boost big_array)