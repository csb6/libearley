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
#include <ranges>
#include <algorithm>
#include <concepts>
#include <cstdint>
#include <cstddef>
#include "span_list.hpp"

namespace earley {

/* Default implementation of symbol_traits for enum class types containing
   a Symbol_Count enum member */
template<std::regular Symbol>
    requires requires {
        { static_cast<uint8_t>(Symbol::Symbol_Count) };
    }
struct symbol_traits {
    static constexpr auto symbol_count = static_cast<uint8_t>(Symbol::Symbol_Count);

    static constexpr
    uint8_t to_index(Symbol s) noexcept { return static_cast<uint8_t>(s); }
};

/* Represents a grammar rule where symbol is the left-hand side
   and components is the right-hand side of the rule. */
template<typename Symbol>
struct Rule {
    Symbol symbol;
    std::vector<Symbol> components;
};

/* Represents a grammar and associated data structures. All interactions
   with the grammar should take place through this object. */
template<typename Symbol>
struct RuleSet {
    using index_range = std::ranges::iota_view<uint16_t, uint16_t>;
    using SymbolTraits = symbol_traits<Symbol>;
    static constexpr auto symbol_count = SymbolTraits::symbol_count;

    explicit constexpr
    RuleSet(std::span<const Rule<Symbol>> rules)
        : rules(rules)
    {
        // Assumes rules are grouped by symbol
        auto progress = rules.begin();
        while(progress != rules.end()) {
            auto symbol = progress->symbol;
            auto start_idx = (uint16_t)(progress - rules.begin());
            progress = std::ranges::find_if_not(progress + 1, rules.end(), [symbol](auto s) { return s == symbol; }, &Rule<Symbol>::symbol);
            rule_spans[SymbolTraits::to_index(symbol)] = {start_idx, (uint16_t)(progress - rules.begin())};
        }

        // Mark all nullable rules
        bool at_fixpoint;
        do {
            at_fixpoint = true;
            for(const auto& rule : rules) {
                if(!is_nullable(rule.symbol)
                 && std::ranges::all_of(rule.components, [this](Symbol s) { return is_nullable(s); })) {
                    nullable[SymbolTraits::to_index(rule.symbol)] = true;
                    at_fixpoint = false;
                }
            }
        } while(!at_fixpoint);
    }

    /* Returns a range of indices [A, B) such that A is the index of the first rule in rules with rule_sym as
       its symbol and B is the index of the rule one past F, where F is the final rule in the grammar with
       rule_sym as its symbol. */
    constexpr
    index_range operator[](Symbol rule_sym) const noexcept { return rule_spans[SymbolTraits::to_index(rule_sym)]; }

    constexpr
    bool is_nullable(Symbol rule_sym) const noexcept { return nullable[SymbolTraits::to_index(rule_sym)]; }

    std::span<const Rule<Symbol>> rules; /* The rules of the grammar */
    index_range rule_spans[SymbolTraits::symbol_count]{};
    bool nullable[SymbolTraits::symbol_count]{};
};

/* Represents a match (partial or complete) of a particular rule starting at a particular
   position in the input. */
struct EarleyItem {
    explicit constexpr
    EarleyItem(uint16_t rule_idx, uint32_t start_pos, uint16_t progress = 0)
        : rule_idx(rule_idx), progress(progress), start_pos(start_pos) {}

    constexpr
    bool operator==(const EarleyItem&) const noexcept = default;

    uint16_t rule_idx;  /* Index of the rule that is being matched */
    uint16_t progress;  /* Dividing point between this item's matched/unmatched components */
    uint32_t start_pos; /* Where in input this match starts */
};

using StateSetIterator = SpanList<EarleyItem>::const_iterator;

using EarleyItemIterator = std::span<const EarleyItem>::iterator;

/* Returns true if the item is complete. Note that rule_comp_count
   is assumed to equal rule.components.size(), where rule is the
   Rule corresponding to item.rule_idx. */
constexpr
bool is_completed(EarleyItem item, uint16_t rule_comp_count)
{
    return item.progress == rule_comp_count;
}

/* Look ahead at the next unmatched component of a rule */
template<typename Symbol>
constexpr
Symbol next_symbol(const Rule<Symbol>& rule, EarleyItem item)
{
    return rule.components[item.progress];
}

/* Returns true if the given item already exists in the given state set */
constexpr
bool item_exists(const std::ranges::forward_range auto& state_set, EarleyItem item)
{
    return std::ranges::find(state_set, item) != state_set.end();
}

/* The Earley recognizer. The output is a list of state sets, each of which contains
   zero or more Earley items. */
template<typename Token, std::regular Symbol, std::ranges::input_range InputRange>
    requires
        std::convertible_to<std::ranges::range_value_t<InputRange>, Token>
     && requires(Symbol s, Token t) {
            { is_terminal(s) } -> std::same_as<bool>;
            { matches_terminal(s, t) } -> std::same_as<bool>;
        }
SpanList<EarleyItem> parse(const RuleSet<Symbol>& rule_set, Symbol start_symbol, size_t max_item_capacity, InputRange&& input)
{
    SpanList<EarleyItem> state_sets{max_item_capacity};
    // Initialize S(0)
    state_sets.add_span();
    for(auto rule_idx : rule_set[start_symbol]) {
        state_sets.emplace_back(rule_idx, 0);
    }

    // Process input
    std::vector<EarleyItem> next_state_set;
    auto curr_token = std::ranges::begin(input);
    auto end_token = std::ranges::end(input);
    for(uint32_t curr_pos = 0; !state_sets[curr_pos].empty(); ++curr_pos, ++curr_token) {
        auto state_set = state_sets.curr_span();
        for(auto item : state_set) {
            const auto& item_rule = rule_set.rules[item.rule_idx];
            if(is_completed(item, item_rule.components.size())) {
                // Completion
                for(auto start_item : state_sets[item.start_pos]) {
                    const auto& start_rule = rule_set.rules[start_item.rule_idx];
                    if(!is_completed(start_item, start_rule.components.size())
                        && next_symbol(start_rule, start_item) == item_rule.symbol
                        && !item_exists(state_set, start_item)) {
                        state_sets.emplace_back(start_item.rule_idx, start_item.start_pos, start_item.progress + 1);
                    }
                }
            } else {
                auto next_sym = next_symbol(item_rule, item);
                if(is_terminal(next_sym) && curr_token != end_token) {
                    // Scan
                    if(matches_terminal(next_sym, *curr_token)) {
                        next_state_set.emplace_back(item.rule_idx, item.start_pos, item.progress + 1);
                    }
                } else {
                    // Prediction
                    EarleyItem predicted_item{0, curr_pos};
                    for(auto rule_idx : rule_set[next_sym]) {
                        predicted_item.rule_idx = rule_idx;
                        if(!item_exists(state_set, predicted_item)) {
                            state_sets.emplace_back(predicted_item);
                        }
                    }
                    // Advance item if it is incomplete and the next symbol is nullable
                    if(rule_set.is_nullable(next_sym)) {
                        predicted_item = item;
                        ++predicted_item.progress;
                        if(!item_exists(state_set, predicted_item)) {
                            state_sets.emplace_back(std::move(predicted_item));
                        }
                    }
                }
            }
        }
        state_sets.add_span();
        state_sets.append(next_state_set.begin(), next_state_set.end());
        next_state_set.clear();
    }
    return state_sets;
}

struct ParseResult {
    constexpr
    ParseResult() = default;
    constexpr
    ParseResult(StateSetIterator state_set, EarleyItemIterator item)
        : state_set(state_set), item(item) {}

    StateSetIterator state_set;
    EarleyItemIterator item;

    constexpr
    operator bool() const noexcept { return item != EarleyItemIterator{}; }
};

/* Finds an Earley item (and its containing state set) that has the given symbol and
   matches the full input. If no such Earley item exists, it returns a result that
   evaluates to false. */
template<typename Symbol>
constexpr
ParseResult find_full_parse(std::span<const Rule<Symbol>> rules, Symbol symbol,
                            const SpanList<EarleyItem>& state_sets, std::ranges::input_range auto&& input)
{
    if(state_sets.size() < input.size()) {
        return {};
    }

    auto state_set = state_sets.begin() + input.size();
    auto full_parse = std::ranges::find_if(*state_set, [&rules, symbol](const auto& item) {
        const auto& rule = rules[item.rule_idx];
        return is_completed(item, rule.components.size())
            && item.start_pos == 0
            && rule.symbol == symbol;
    });
    if(full_parse == state_set->end()) {
        return {};
    }
    return {state_set, full_parse};
}

/* Find a completed Earley item with the given symbol as its left-hand side in the provided range of
   Earley items */
template<typename Symbol>
constexpr
EarleyItemIterator find_completed_item(std::span<const Rule<Symbol>> rules,
                                       EarleyItemIterator first, EarleyItemIterator limit, Symbol comp_sym)
{
    return std::ranges::find_if(first, limit, [rules, comp_sym](const auto& item) {
        const auto& item_rule = rules[item.rule_idx];
        // TODO: add another condition to ensure the item we match starts at the input position
        //  we expect. Otherwise we can accidently find 'Number -> [0-9] . (1)' instead of the correct
        //  parse, 'Number -> [0-9] Number . (0)' in the parse for '11'
        //    - This won't be possible to do since we don't know how long a 'Number' should be without
        //      trying one path and backtracking if it doesn't work. Might make sense to see if we can
        //      construct the tree forwards instead like in the article
        //         - If parent item is 'Factor -> Number . (0)', then we DO know where 'Number' has to
        //           start: position 0 in the input. But will this solve the issue in general for
        //           unambiguous parses?
        //    - Could also rely on setting rule priorities, but I don't like this since the grammar in
        //      this case is not ambiguous and works if you use left-recursion instead of right-recursion.
        //      Grammars that have a complete parse should 'just work'
        return item_rule.symbol == comp_sym && is_completed(item, item_rule.components.size());
    });
}

/* Given that we are iterating in reverse over the direct subcomponents of an Earley item and
the current subcomponent is a terminal symbol, advance the state set iterator to the state
set relevant for the next subcomponent of our traversal. */
constexpr
void advance_from_terminal(StateSetIterator& state_set)
{
    --state_set;
}

/* Given that we are iterating in reverse over the direct subcomponents of an Earley item and
the current subcomponent is a nonterminal, advance the state set iterator to the state set
relevant for the next subcomponent in the traversal. */
constexpr
void advance_from_nonterminal(const SpanList<EarleyItem>& state_sets, StateSetIterator& state_set, EarleyItemIterator item)
{
    state_set = state_sets.begin() + item->start_pos;
}

} // namespace earley