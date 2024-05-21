/*
Libearley parser library
Copyright (C) 2024  Cole Blakley

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/
#include "big_array.hpp"

#if defined(unix) || defined(__unix) || defined(__unix__) || defined(__APPLE__)
#include <cassert>
#include <cerrno>
#include <cstring>
#include <sys/mman.h>
#include <unistd.h>

detail::BigArrayBase::BigArrayBase(size_t capacity, size_t element_size)
{
    m_byte_capacity = capacity * element_size;
    m_byte_capacity += m_byte_capacity % (size_t)getpagesize();
    void* data = mmap(nullptr, m_byte_capacity, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, 0, 0);
    if(data == MAP_FAILED) {
        throw std::runtime_error(strerror(errno));
    }
    m_data = (char*)data;
    m_end = m_data;
}

detail::BigArrayBase::~BigArrayBase() noexcept
{
    // Note: element destructors not called
    if(m_data != nullptr) {
        int err = munmap((void*)m_data, m_byte_capacity);
        assert(err == 0);
    }
}

#else

#error "Platform not supported"

#endif /* ifdef (unix or apple platform) */