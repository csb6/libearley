#pragma once

#include <vector>
#include <span>
#include <cassert>
#include <cstdint>
#include <ranges>
#include <boost/stl_interfaces/iterator_interface.hpp>

/* Iterator for moving between spans within a SpanList<T> structure */
template<typename T>
struct SpanListIterator : public boost::stl_interfaces::proxy_iterator_interface<SpanListIterator<T>,
                                    std::random_access_iterator_tag,
                                    std::span<const T>> {
    using Base = boost::stl_interfaces::proxy_iterator_interface<SpanListIterator<T>,
                    std::random_access_iterator_tag,
                    std::span<const T>>;

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

/* Iterator for moving between items within a single span of a SpanList<T> structure.
   Note: this iterator remains valid even when items are added to the SpanList. */
template<typename T>
struct SpanItemIterator : public boost::stl_interfaces::iterator_interface<SpanItemIterator<T>,
                                    std::forward_iterator_tag,
                                    T> {
    using Base = boost::stl_interfaces::iterator_interface<SpanItemIterator,
                    std::forward_iterator_tag,
                    T>;
    using Base::operator++;

    constexpr
    SpanItemIterator() = default;
    constexpr
    SpanItemIterator(std::vector<T>& items, uint32_t idx)
        : items(&items), idx(idx) {}

    constexpr
    bool operator==(const SpanItemIterator& other) const noexcept { return other.idx == idx; }
    constexpr
    T& operator*() noexcept { return (*items)[idx]; }
    constexpr
    T& operator*() const noexcept { return (*items)[idx]; }
    constexpr
    SpanItemIterator& operator++() noexcept
    {
        ++idx;
        return *this;
    }
private:
    friend struct SpanEnd;
    std::vector<T>* items = nullptr;
    uint32_t idx = 0;
};

/* Sentinel for SpanItemIterator representing end of the span */
struct SpanEnd {
    constexpr
    SpanEnd() = default;
    constexpr
    SpanEnd(std::vector<uint32_t>& start_points, uint32_t end_idx)
        : start_points(&start_points), end_idx(end_idx) {}

    template<typename T>
    constexpr
    bool operator==(const SpanItemIterator<T>& iter) const noexcept
    {
        return iter.idx >= (*start_points)[end_idx];
    }
private:
    std::vector<uint32_t>* start_points;
    uint32_t end_idx = 0;
};

/* Represents a growable array (divided into subspans) of items of type T. Items can only be added to
   the last subspan, but all subspans can be read from. All items belong to exactly one subspan. */
template<typename T>
class SpanList {
public:
    using const_iterator = SpanListIterator<T>;
    using item_iterator = SpanItemIterator<T>;
    using item_range = std::ranges::subrange<item_iterator, SpanEnd>;

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
    void insert(Iterator begin, Iterator end)
    {
        start_points.back() += end - begin;
        items.insert(items.end(), begin, end);
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
    /* Same as operator[], but iterators remain valid when items are added to the SpanList */
    constexpr
    item_range span_at(uint32_t index) noexcept
    {
        return {item_iterator{items, start_points[index]}, SpanEnd{start_points, index+1}};
    }

    constexpr
    size_t size() const noexcept { return start_points.size(); }
    constexpr
    const_iterator begin() const noexcept { return {items, start_points.begin()}; }
    constexpr
    const_iterator end() const noexcept { return {items, start_points.end() - 1}; }
private:
    std::vector<T> items;
    std::vector<uint32_t> start_points;
};
