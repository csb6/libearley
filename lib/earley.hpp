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

template<typename Symbol>
    requires requires {
        { static_cast<uint8_t>(Symbol::Symbol_Count) };
    }
constexpr
uint8_t get_symbol_count()
{
    return static_cast<uint8_t>(Symbol::Symbol_Count);
}

template<typename Symbol>
struct Rule {
    Symbol symbol;
    std::vector<Symbol> components;
};

template<typename Symbol>
struct RuleSet {
    using index_range = std::ranges::iota_view<uint16_t, uint16_t>;
    static constexpr auto symbol_count = get_symbol_count<Symbol>();

    explicit constexpr
    RuleSet(std::span<const Rule<Symbol>> rules)
        : rules(rules)
    {
        // Assumes the contents of rules is grouped by symbol
        auto progress = rules.begin();
        while(progress != rules.end()) {
            auto symbol = progress->symbol;
            auto start_idx = (uint16_t)(progress - rules.begin());
            progress = std::ranges::find_if_not(progress + 1, rules.end(), [symbol](auto s) { return s == symbol; }, &Rule<Symbol>::symbol);
            rule_spans[(uint8_t)symbol] = {start_idx, (uint16_t)(progress - rules.begin())};
        }

        // Find all nullable rules
        bool at_fixpoint;
        do {
            at_fixpoint = true;
            for(const auto& rule : rules) {
                if(!is_nullable(rule.symbol)) {
                    if(std::ranges::all_of(rule.components, [this](Symbol s) { return is_nullable(s); })) {
                        nullable[(uint8_t)rule.symbol] = true;
                        at_fixpoint = false;
                    }
                }
            }
        } while(!at_fixpoint);
    }

    constexpr
    index_range operator[](Symbol rule_sym) const noexcept { return rule_spans[(uint8_t)rule_sym]; }

    constexpr
    bool is_nullable(Symbol rule_sym) const noexcept { return nullable[(uint8_t)rule_sym]; }

    std::span<const Rule<Symbol>> rules;
    index_range rule_spans[symbol_count]{};
    bool nullable[symbol_count]{};
};

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

/* Returns true if the item is complete. */
constexpr
bool is_completed(const EarleyItem& item, uint16_t rule_comp_count)
{
    return item.progress == rule_comp_count;
}

template<typename Symbol>
constexpr
Symbol next_symbol(const Rule<Symbol>& rule, const EarleyItem& item)
{
    return rule.components[item.progress];
}

constexpr
bool item_exists(const std::ranges::forward_range auto& state_set, const EarleyItem& item)
{
    return std::ranges::find(state_set, item) != state_set.end();
}

template<typename Token, std::regular Symbol, std::ranges::input_range InputRange>
    requires
        std::convertible_to<std::ranges::range_value_t<InputRange>, Token>
     && requires(Symbol s, Token t) {
            { is_terminal(s) } -> std::same_as<bool>;
            { matches_terminal(s, t) } -> std::same_as<bool>;
        }
constexpr
SpanList<EarleyItem> parse(const RuleSet<Symbol>& rule_set, Symbol start_symbol, InputRange&& input)
{
    SpanList<EarleyItem> state_sets;
    // Initialize S(0)
    state_sets.add_span();
    for(auto rule_idx : rule_set[start_symbol]) {
        state_sets.emplace_back(rule_idx, 0);
    }

    // Process input
    std::vector<EarleyItem> next_state_set;
    auto curr_token = std::ranges::begin(input);
    auto end_token = std::ranges::end(input);
    for(uint32_t curr_pos = 0; !state_sets[curr_pos].empty(); ++curr_pos) {
        auto state_set = state_sets.span_at(curr_pos);
        for(const auto& item : state_set) {
            const auto& item_rule = rule_set.rules[item.rule_idx];
            if(is_completed(item, item_rule.components.size())) {
                // Completion
                for(const auto& start_item : state_sets.span_at(item.start_pos)) {
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
        state_sets.insert(next_state_set.begin(), next_state_set.end());
        next_state_set.clear();
        ++curr_token;
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

template<typename Symbol>
constexpr
bool is_full_parse(std::span<const Rule<Symbol>> rules, Symbol start_symbol, const EarleyItem& item)
{
    const auto& rule = rules[item.rule_idx];
    return is_completed(item, rule.components.size()) && item.start_pos == 0 && rule.symbol == start_symbol;
}

template<typename Symbol>
constexpr
ParseResult find_full_parse(std::span<const Rule<Symbol>> rules, Symbol start_symbol,
                            const SpanList<EarleyItem>& state_sets, std::ranges::input_range auto&& input)
{
    if(state_sets.size() < input.size()) {
        return {};
    }

    auto state_set = state_sets.begin() + input.size();
    auto full_parse = std::ranges::find_if(*state_set, [&rules, start_symbol](const auto& item) {
        return is_full_parse(rules, start_symbol, item);
    });
    if(full_parse == state_set->end()) {
        return {};
    }
    return {state_set, full_parse};
}

/* Find a completed Earley item with the given symbol as its head in the provided range of Earley items */
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