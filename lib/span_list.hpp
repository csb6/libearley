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

#include <vector>
#include <span>
#include <cassert>
#include <cstdint>
#include <ranges>
#include <boost/stl_interfaces/iterator_interface.hpp>
#include "big_array.hpp"

/* Iterator for moving between spans within a SpanList<T> structure */
template<typename T>
struct SpanListIterator : public boost::stl_interfaces::proxy_iterator_interface<SpanListIterator<T>,
                                    std::random_access_iterator_tag,
                                    std::span<const T>> {
    constexpr
    SpanListIterator() = default;
    constexpr
    SpanListIterator(std::span<const T> items, std::vector<uint32_t>::const_iterator start_point)
        : items(items), start_point(start_point) {}

    constexpr
    bool operator==(const SpanListIterator& other) const noexcept { return start_point == other.start_point; }
    constexpr
    std::span<const T> operator*() const noexcept
    {
        return {items.begin() + *start_point, items.begin() + *(start_point + 1)};
    }
    constexpr
    std::ptrdiff_t operator-(const SpanListIterator& other) const noexcept { return start_point - other.start_point; }
    constexpr
    SpanListIterator& operator+=(size_t n) noexcept
    {
        start_point += n;
        return *this;
    }
private:
    std::span<const T> items;
    std::vector<uint32_t>::const_iterator start_point;
};

/* Sentinel representing end of a span of items */
template<typename T>
struct SpanEnd {
    constexpr
    SpanEnd() = default;
    constexpr
    SpanEnd(const BigArray<T>* end)
        : end(end) {}

    bool operator==(const T* pos) const noexcept
    {
        return pos >= end->end();
    }
private:
    const BigArray<T>* end = nullptr;
};

/* Represents a growable array (divided into subspans) of items of type T. Items can only be added to
   the last subspan, but all subspans can be read from. All items belong to exactly one subspan. */
template<typename T>
class SpanList {
public:
    using const_iterator = SpanListIterator<T>;

    explicit
    SpanList(size_t item_capacity)
        : items(item_capacity) {}

    constexpr
    void add_span()
    {
        // Initial span is an empty span [N, N)
        if(start_points.empty()) {
            start_points.push_back(0);
            start_points.push_back(0);
        } else {
            start_points.push_back(start_points.back());
        }
    }

    template<typename Iterator>
    constexpr
    void append(Iterator begin, Iterator end)
    {
        start_points.back() += end - begin;
        items.append(begin, end);
    }

    template<typename ...Args>
    constexpr
    void emplace_back(Args&&... args)
    {
        assert(!start_points.empty());
        items.emplace_back(std::forward<Args&&>(args)...);
        ++start_points.back();
    }

    constexpr
    std::span<const T> operator[](uint32_t index) const noexcept
    {
        return {items.begin() + start_points[index], items.begin() + start_points[index+1]};
    }
    /* Iterators remain valid when items are added to the SpanList
       Note: adding new spans invalidates the ranges returned by this function. */
    std::ranges::subrange<T*, SpanEnd<T>> curr_span() noexcept
    {
        return {items.begin() + *(start_points.end() - 2), SpanEnd{&items}};
    }

    constexpr
    size_t num_of_items() const noexcept { return items.size(); }
    constexpr
    size_t size() const noexcept { return start_points.size(); }
    constexpr
    const_iterator begin() const noexcept { return {items, start_points.begin()}; }
    constexpr
    const_iterator end() const noexcept { return {items, start_points.end() - 1}; }
private:
    BigArray<T> items;
    std::vector<uint32_t> start_points;
};
