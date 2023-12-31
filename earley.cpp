#include <ostream>
#include <iostream>
#include <cassert>
#include <span>
#include <string_view>
#include <optional>
#include <vector>
#include <cstdint>
#include <utility>
#include <algorithm>
#include <concepts>
#include <numeric>

enum class Symbol : uint8_t {
    /* Terminals */
    Plus, Minus, Mult, Div, LParen, RParen, Digit,
    /* Nonterminals */
    Number, Sum, Product, Factor,

    Last_Terminal = Digit
};

struct Rule {
    Symbol symbol;
    std::vector<Symbol> components;
};

struct EarleyItem {
    explicit
    EarleyItem(uint16_t rule_idx, uint32_t start_pos, uint16_t progress = 0)
        : rule_idx(rule_idx), progress(progress), start_pos(start_pos) {}

    uint16_t rule_idx;  /* Which rule is matched */
    uint16_t progress;  /* Dividing point between this item's matched/unmatched components */
    uint32_t start_pos; /* Where in input this match starts */
};

using StateSet = std::vector<EarleyItem>;
using StateSetView = std::span<const EarleyItem>;
using EarleyItemIterator = StateSet::const_iterator;

static
std::ostream& operator<<(std::ostream&, Symbol);

static
std::ostream& print_item(std::ostream&, std::span<const Rule>, const EarleyItem&);

static
std::ostream& print_state_set(std::ostream&, std::span<const Rule>, StateSetView);

static constexpr
bool is_terminal(Symbol s) { return (uint8_t)s <= (uint8_t)Symbol::Last_Terminal; }

static constexpr
bool matches_terminal(Symbol terminal, char input)
{
    using enum Symbol;
    switch(terminal) {
        case Plus:   return input == '+';
        case Minus:  return input == '-';
        case Mult:   return input == '*';
        case Div:    return input == '/';
        case LParen: return input == '(';
        case RParen: return input == ')';
        case Digit:  return std::isdigit(input);
        default:     return false;
    }
}

static constexpr
bool is_completed(const Rule& rule, const EarleyItem& item)
{
    return item.progress == rule.components.size();
}

static constexpr
bool is_full_parse(std::span<const Rule> rules, Symbol start_symbol, const EarleyItem& item)
{
    const auto& rule = rules[item.rule_idx];
    return is_completed(rule, item) && item.start_pos == 0 && rule.symbol == start_symbol;
}

static constexpr
Symbol next_symbol(const Rule& rule, const EarleyItem& item)
{
    return rule.components[item.progress];
}

static constexpr
bool rule_exists(StateSetView state_set, uint16_t rule_idx, uint32_t curr_pos)
{
    return std::ranges::any_of(state_set, [rule_idx, curr_pos](const auto& item) {
        return item.rule_idx == rule_idx && item.start_pos == curr_pos;
    });
}

static constexpr
std::vector<StateSet> parse(std::span<const Rule> rules, Symbol start_symbol, std::string_view input)
{
    std::vector<StateSet> state_sets;
    // Initialize S(0)
    state_sets.emplace_back();
    for(uint16_t i = 0; i < rules.size(); ++i) {
        if(rules[i].symbol == start_symbol) {
            state_sets[0].emplace_back(i, 0);
        }
    }

    // Process input
    for(uint32_t curr_pos = 0; curr_pos <= input.size() && !state_sets[curr_pos].empty(); ++curr_pos) {
        state_sets.emplace_back();
        auto& state_set = state_sets[curr_pos];
        for(uint32_t i = 0; i < state_set.size(); ++i) {
            const auto& item = state_set[i];
            const auto& item_rule = rules[item.rule_idx];
            if(is_completed(item_rule, item)) {
                // Completion
                for(const auto& start_item : state_sets[item.start_pos]) {
                    const auto& start_rule = rules[start_item.rule_idx];
                    if(!is_completed(start_rule, start_item) && next_symbol(start_rule, start_item) == item_rule.symbol) {
                        state_set.emplace_back(start_item.rule_idx, start_item.start_pos, start_item.progress + 1);
                    }
                }
            } else {
                auto next_sym = next_symbol(item_rule, item);
                if(is_terminal(next_sym) && curr_pos < input.size()) { // TODO: how can we get rid of this check (only necessary for final state)
                    // Scan
                    if(matches_terminal(next_sym, input[curr_pos])) {
                        state_sets[curr_pos + 1].emplace_back(item.rule_idx, item.start_pos, item.progress + 1);
                    }
                } else {
                    // Prediction
                    for(uint16_t rule_idx = 0; rule_idx < rules.size(); ++rule_idx) {
                        if(rules[rule_idx].symbol == next_sym && !rule_exists(state_set, rule_idx, curr_pos)) {
                            state_set.emplace_back(rule_idx, curr_pos);
                        }
                    }
                }
            }
        }
    }
    return state_sets;
}

struct ParseState {
    constexpr
    ParseState() = default;
    constexpr
    ParseState(std::span<const StateSet>::iterator state_set, const Rule& rule)
        : state_set(state_set), rule(&rule) {}

    std::span<const StateSet>::iterator state_set;
    const Rule* rule = nullptr;

    constexpr
    operator bool() const noexcept { return rule != nullptr; }
};

static constexpr
ParseState find_final_parse(std::span<const Rule> rules, Symbol start_symbol,
                            std::span<const StateSet> state_sets, std::string_view input)
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

    return {state_set, rules[full_parse->rule_idx]};
}

static constexpr
EarleyItemIterator next_from_terminal(ParseState& state)
{
    --state.state_set;
    return state.state_set->end();
}

static constexpr
EarleyItemIterator next_from_nonterminal(std::span<const Rule> rules, ParseState& state,
                                         std::span<const StateSet> state_sets,
                                         EarleyItemIterator curr, EarleyItemIterator limit,
                                         std::vector<Symbol>::const_reverse_iterator comp_sym)
{
    auto comp_item = std::ranges::find_if(curr, limit, [&rules, comp_sym](const auto& item) {
        const auto& item_rule = rules[item.rule_idx];
        return item_rule.symbol == *comp_sym && is_completed(item_rule, item);
    });
    assert(comp_item != state.state_set->end());
    state.state_set = state_sets.begin() + comp_item->start_pos;
    return comp_item;
}

static constexpr
EarleyItemIterator next_parse(std::span<const Rule> rules, std::span<const StateSet> state_sets, ParseState& state,
                              std::vector<Symbol>::const_reverse_iterator comp_sym)
{
    if(is_terminal(*comp_sym)) {
        --state.state_set;
        return state.state_set->end();
    } else {
        // Nonterminal
        // TODO: split up this function so we can continue search in state.state_set if we want to,
        //  allowing us to try different alternatives
        auto comp_item = std::ranges::find_if(*state.state_set, [&rules, comp_sym](const auto& item) {
            const auto& item_rule = rules[item.rule_idx];
            return item_rule.symbol == *comp_sym && is_completed(item_rule, item);
        });
        assert(comp_item != state.state_set->end());
        state.state_set = state_sets.begin() + comp_item->start_pos;
        return comp_item;
    }
}

static constexpr
bool build_parse_tree(std::span<const Rule> rules, Symbol start_symbol,
                      std::span<const StateSet> state_sets, std::string_view input)
{
    if(state_sets.size() < input.size()) {
        return false;
    }

    auto state_set = state_sets.begin() + input.size();
    auto full_parse = std::ranges::find_if(*state_set, [&rules, start_symbol](const auto& item) {
        return is_full_parse(rules, start_symbol, item);
    });
    if(full_parse == state_set->end()) {
        return false;
    }

    std::cerr << "Full parse: "; print_item(std::cerr, rules, *full_parse) << "\n\n";
    const auto& full_parse_rule = rules[full_parse->rule_idx];

    for(auto comp_sym = full_parse_rule.components.rbegin(); comp_sym < full_parse_rule.components.rend(); ++comp_sym) {
        if(is_terminal(*comp_sym)) {
            std::cerr << "Partial parse: " << *comp_sym << "\n";
            --state_set;
        } else {
            // Nonterminal
            auto comp_item = std::ranges::find_if(*state_set, [&rules, comp_sym](const auto& item) {
                const auto& item_rule = rules[item.rule_idx];
                return item_rule.symbol == *comp_sym && is_completed(item_rule, item);
            });
            assert(comp_item != state_set->end());
            std::cerr << "Partial parse: "; print_item(std::cerr, rules, *comp_item) << "\n";
            state_set = state_sets.begin() + comp_item->start_pos;
        }
    }

    return true;
}


int main(int argc, char** argv)
{
    if(argc != 2) {
        std::cerr << "Usage: " << argv[0] << " input\n";
        return 1;
    }

    using enum Symbol;
    const Rule rules[] = {
        // Sum     -> Sum     [+ -] Product
        { Sum,     { Sum, Plus, Product } },
        { Sum,     { Sum, Minus, Product } },
        // Sum     -> Product
        { Sum,     { Product } },
        // Product -> Product [* /] Factor
        { Product, { Product, Mult, Factor } },
        { Product, { Product, Div, Factor } },
        // Product -> Factor
        { Product, { Factor } },
        // Factor  -> '(' Sum ')'
        { Factor,  { LParen, Sum, RParen } },
        // Factor  -> Number
        { Factor,  { Number } },
        // Number  -> [0-9]
        { Number,  { Digit } },
        // Number  -> [0-9] Number
        { Number,  { Digit, Number } }
    };
    constexpr auto start_symbol = Sum;

    std::string_view input = argv[1];
    auto state_sets = parse(rules, start_symbol, input);

    std::cerr << "\nState sets after parsing terminates:\n";
    uint32_t state_set_num = 0;
    for(const auto& state_set : state_sets) {
        std::cerr << "S(" << state_set_num++ << "):\n";
        print_state_set(std::cerr, rules, state_set) << "\n";
    }

    // if(!build_parse_tree(rules, start_symbol, state_sets, input)) {
    //     std::cerr << "Error: parse failed\n";
    // }
    auto parse_state = find_final_parse(rules, start_symbol, state_sets, input);
    if(!parse_state) {
        std::cerr << "Error: parse failed\n";
        return 1;
    }

    for(auto comp_sym = parse_state.rule->components.rbegin(); comp_sym < parse_state.rule->components.rend(); ++comp_sym) {
        EarleyItemIterator item = parse_state.state_set->begin();
        if(is_terminal(*comp_sym)) {
            item = next_from_terminal(parse_state);
            std::cerr << "Partial parse: " << *comp_sym << "\n";
        } else {
            item = next_from_nonterminal(rules, parse_state, state_sets, item, parse_state.state_set->end(), comp_sym);
            std::cerr << "Partial parse: "; print_item(std::cerr, rules, *item) << "\n";
        }
    }
    return 0;
}

static
std::ostream& operator<<(std::ostream& out, Symbol s)
{
    using enum Symbol;
    switch(s) {
        case Plus:
            out << "'+'";
            break;
        case Minus:
            out << "'-'";
            break;
        case Mult:
            out << "'*'";
            break;
        case Div:
            out << "'/'";
            break;
        case LParen:
            out << "'('";
            break;
        case RParen:
            out << "')'";
            break;
        case Digit:
            out << "[0-9]";
            break;
        case Number:
            out << "Number";
            break;
        case Sum:
            out << "Sum";
            break;
        case Product:
            out << "Product";
            break;
        case Factor:
            out << "Factor";
            break;
    }
    return out;
}

static
std::ostream& print_item(std::ostream& out, std::span<const Rule> rules, const EarleyItem& item)
{
    auto& rule = rules[item.rule_idx];
    out << rule.symbol << " -> ";
    for(uint16_t i = 0; i < rule.components.size(); ++i) {
        if(i == item.progress) {
            out << ". ";
        }
        out << rule.components[i] << " ";
    }
    if(is_completed(rule, item)) {
        out << ". ";
    }
    out << "(" << item.start_pos << ")";
    return out;
}

static
std::ostream& print_state_set(std::ostream& out, std::span<const Rule> rules, StateSetView state_set)
{
    out << "{\n";
    for(const auto& item : state_set) {
        out << "  ";
        print_item(out, rules, item) << "\n";
    }
    return out << "}";
}