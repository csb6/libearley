cmake_minimum_required(VERSION 3.12)

project(earley LANGUAGES CXX)

find_package(Boost REQUIRED)

add_executable(earley earley.cpp)
target_link_libraries(earley PUBLIC Boost::boost)