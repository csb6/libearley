#include "big_array.hpp"
#include <iostream>
#include <cassert>
#include <unistd.h>

int main()
{
    BigArray<int> array{1'000'000'000'000};

    assert(array.size() == 0);
    array.push_back(5);
    assert(array.size() == 1);
    array.push_back(89);
    assert(array.size() == 2);
    array.push_back(70);
    assert(array.size() == 3);
    for(int i : array) {
        std::cout << i << " ";
    }
    assert(array[0] == 5);
    assert(array[1] == 89);
    assert(array[2] == 70);
    std::cout << "\n";

    std::cout << "Bytes reserved: " << array.byte_capacity() << "\n";
    std::cout << "Pages reserved: " << array.byte_capacity() / (size_t)getpagesize() << "\n";

    return 0;
}