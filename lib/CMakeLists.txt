cmake_minimum_required(VERSION 3.12)

add_library(libearley INTERFACE earley.hpp earley_print.hpp)
target_include_directories(libearley INTERFACE .)
target_link_libraries(libearley INTERFACE Boost::boost)