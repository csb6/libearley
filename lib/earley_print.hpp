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

#include "earley.hpp"
#include <ostream>

namespace earley {

template<typename Symbol>
std::ostream& print_item(std::ostream& out, std::span<const Rule<Symbol>> rules, EarleyItem item)
{
    auto& rule = rules[item.rule_idx];
    out << rule.symbol << " -> ";
    for(uint16_t i = 0; i < rule.components.size(); ++i) {
        if(i == item.progress) {
            out << ". ";
        }
        out << rule.components[i] << " ";
    }
    if(is_completed(item, rule.components.size())) {
        out << ". ";
    }
    out << "(" << item.start_pos << ")";
    return out;
}

template<typename Symbol>
std::ostream& print_state_set(std::ostream& out, std::span<const Rule<Symbol>> rules, std::span<const earley::EarleyItem> state_set)
{
    out << "{\n";
    for(const auto& item : state_set) {
        out << "  ";
        print_item(out, rules, item) << "\n";
    }
    return out << "}";
}

} // namespace earley