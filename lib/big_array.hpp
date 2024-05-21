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
#pragma once

#include <type_traits>
#include <stdexcept>

namespace detail {

class BigArrayBase {
protected:
    BigArrayBase(size_t capacity, size_t element_size);
public:
    BigArrayBase(const BigArrayBase&) = delete;
    BigArrayBase& operator=(const BigArrayBase&) = delete;
    BigArrayBase(BigArrayBase&& other) noexcept
        : m_byte_capacity(other.m_byte_capacity), m_data(other.m_data), m_end(other.m_end)
    {
        other.m_byte_capacity = 0;
        other.m_data = nullptr;
        other.m_end = nullptr;
    }
    BigArrayBase& operator=(BigArrayBase&& other) noexcept
    {
        m_byte_capacity = other.m_byte_capacity;
        m_data = other.m_data;
        m_end = other.m_end;
        other.m_byte_capacity = 0;
        other.m_data = nullptr;
        other.m_end = nullptr;
        return *this;
    }
    ~BigArrayBase() noexcept;

    size_t byte_capacity() const noexcept { return m_byte_capacity; }
protected:
    void check_has_space(size_t element_size, size_t count = 1) const
    {
        if(m_end + element_size * count > m_data + m_byte_capacity) {
            throw std::runtime_error("BigArray is out of memory");
        }
    }

    size_t m_byte_capacity;
    char* m_data;
    char* m_end;
};

} // namespace detail

template<typename T>
    requires std::is_trivially_destructible_v<T>
class BigArray : public detail::BigArrayBase {
public:
    using iterator = T*;
    using const_iterator = const T*;

    explicit
    BigArray(size_t capacity)
        : BigArrayBase(capacity, sizeof(T)) {}

    T& push_back(T&& element)
    {
        check_has_space(sizeof(T));
        T* result = new (m_end) T{std::move(element)};
        m_end += sizeof(T);
        return *result;
    }

    template<typename ...Args>
    T& emplace_back(Args&&... args)
    {
        check_has_space(sizeof(T));
        T* result = new (m_end) T(std::forward<Args>(args)...);
        m_end += sizeof(T);
        return *result;
    }

    template<typename Iterator>
    void append(Iterator first, Iterator limit)
    {
        check_has_space(sizeof(T), limit - first);
        for(auto it = first; it != limit; ++it) {
            new (m_end) T{*it};
            m_end += sizeof(T);
        }
    }

    iterator       begin() noexcept       { return (T*)m_data; }
    const_iterator begin() const noexcept { return (const T*)m_data; }
    iterator       end() noexcept         { return (T*)m_end; }
    const_iterator end() const noexcept   { return (const T*)m_end; }
    T&       back() noexcept       { return *(end() - 1); }
    const T& back() const noexcept { return *(end() - 1); }
    T&       operator[](size_t index) noexcept       { return begin()[index]; }
    const T& operator[](size_t index) const noexcept { return begin()[index]; }
    size_t size() const noexcept { return end() - begin(); }
};