cmake_minimum_required(VERSION 3.12)

add_executable(example example.cpp)
target_link_libraries(example PUBLIC libearley)

add_executable(test_nullable test_nullable.cpp)
target_link_libraries(test_nullable PUBLIC libearley)

add_executable(test_input_file test_input_file.cpp)
target_link_libraries(test_input_file PUBLIC libearley)

add_executable(test_big_array test_big_array.cpp)
target_link_libraries(test_big_array PUBLIC big_array)