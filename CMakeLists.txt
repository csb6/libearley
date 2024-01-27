cmake_minimum_required(VERSION 3.12)

project(earley LANGUAGES CXX)

find_package(Boost REQUIRED)

add_subdirectory(lib)
add_subdirectory(test)