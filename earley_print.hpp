#pragma once

#include "earley.hpp"
#include <ostream>

namespace earley {

template<typename Symbol>
std::ostream& print_item(std::ostream& out, std::span<const Rule<Symbol>> rules, const earley::EarleyItem& item)
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