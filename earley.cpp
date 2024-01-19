#include <iostream>
#include <cassert>
#include <span>
#include <string_view>
#include <vector>
#include <cstdint>
#include <algorithm>
#include <cctype>
#include <ratio>
#include <chrono>
#include "span_list.hpp"

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
    explicit constexpr
    EarleyItem(uint16_t rule_idx, uint32_t start_pos, uint16_t progress = 0)
        : rule_idx(rule_idx), progress(progress), start_pos(start_pos) {}

    uint16_t rule_idx;  /* Index of the rule that is being matched */
    uint16_t progress;  /* Dividing point between this item's matched/unmatched components */
    uint32_t start_pos; /* Where in input this match starts */
};

using StateSetIterator = SpanList<EarleyItem>::const_iterator;

using StateSet = std::span<const EarleyItem>;

using EarleyItemIterator = StateSet::iterator;

static
std::ostream& operator<<(std::ostream&, Symbol);

static
std::ostream& print_item(std::ostream&, std::span<const Rule>, const EarleyItem&);

static
std::ostream& print_state_set(std::ostream&, std::span<const Rule>, StateSet);

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

/* Returns true if the item is complete. rule must be the item referred to
by item.rule_idx */
static constexpr
bool is_completed(const Rule& rule, const EarleyItem& item)
{
    return item.progress == rule.components.size();
}

static constexpr
Symbol next_symbol(const Rule& rule, const EarleyItem& item)
{
    return rule.components[item.progress];
}

// TODO: state_set is 32 bytes in size. Is it possible to make it smaller/cheaper to copy?
//  Consider possible "fat" range object with small iterators that point back to it
//  (see https://ericniebler.com/2013/11/07/input-iterators-vs-input-ranges/)
static constexpr
bool rule_exists(const std::ranges::forward_range auto& state_set, uint16_t rule_idx, uint32_t curr_pos)
{
    return std::ranges::any_of(state_set, [rule_idx, curr_pos](const auto& item) {
        return item.rule_idx == rule_idx && item.start_pos == curr_pos;
    });
}

static constexpr
SpanList<EarleyItem> parse(std::span<const Rule> rules, Symbol start_symbol, std::string_view input)
{
    SpanList<EarleyItem> state_sets;
    // Initialize S(0)
    state_sets.add_span();
    for(uint16_t i = 0; i < rules.size(); ++i) {
        if(rules[i].symbol == start_symbol) {
            state_sets.emplace_back(i, 0);
        }
    }

    // Process input
    std::vector<EarleyItem> next_state_set;
    for(uint32_t curr_pos = 0; curr_pos <= input.size() && !state_sets[curr_pos].empty(); ++curr_pos) {
        auto state_set = state_sets.span_at(curr_pos);
        for(const auto& item : state_set) {
            const auto& item_rule = rules[item.rule_idx];
            if(is_completed(item_rule, item)) {
                // Completion
                for(const auto& start_item : state_sets.span_at(item.start_pos)) {
                    const auto& start_rule = rules[start_item.rule_idx];
                    if(!is_completed(start_rule, start_item) && next_symbol(start_rule, start_item) == item_rule.symbol) {
                        state_sets.emplace_back(start_item.rule_idx, start_item.start_pos, start_item.progress + 1);
                    }
                }
            } else {
                auto next_sym = next_symbol(item_rule, item);
                if(is_terminal(next_sym) && curr_pos < input.size()) { // TODO: how can we get rid of this check (only necessary for final state)
                    // Scan
                    if(matches_terminal(next_sym, input[curr_pos])) {
                        next_state_set.emplace_back(item.rule_idx, item.start_pos, item.progress + 1);
                    }
                } else {
                    // Prediction
                    for(uint16_t rule_idx = 0; rule_idx < rules.size(); ++rule_idx) {
                        if(rules[rule_idx].symbol == next_sym && !rule_exists(state_set, rule_idx, curr_pos)) {
                            state_sets.emplace_back(rule_idx, curr_pos);
                        }
                    }
                }
            }
        }
        state_sets.add_span();
        state_sets.insert(next_state_set.begin(), next_state_set.end());
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

static constexpr
bool is_full_parse(std::span<const Rule> rules, Symbol start_symbol, const EarleyItem& item)
{
    const auto& rule = rules[item.rule_idx];
    return is_completed(rule, item) && item.start_pos == 0 && rule.symbol == start_symbol;
}

static constexpr
ParseResult find_full_parse(std::span<const Rule> rules, Symbol start_symbol,
                            const SpanList<EarleyItem>& state_sets, std::string_view input)
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
static constexpr
EarleyItemIterator find_completed_item(std::span<const Rule> rules,
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
        return item_rule.symbol == comp_sym && is_completed(item_rule, item);
    });
}

/* Given that we are iterating in reverse over the direct subcomponents of an Earley item and
the current subcomponent is a terminal symbol, advance the state set iterator to the state
set relevant for the next subcomponent of our traversal. */
static constexpr
void advance_from_terminal(StateSetIterator& state_set)
{
    --state_set;
}

/* Given that we are iterating in reverse over the direct subcomponents of an Earley item and
the current subcomponent is a nonterminal, advance the state set iterator to the state set
relevant for the next subcomponent in the traversal. */
static constexpr
void advance_from_nonterminal(const SpanList<EarleyItem>& state_sets, StateSetIterator& state_set, EarleyItemIterator item)
{
    state_set = state_sets.begin() + item->start_pos;
}

static
void print_elapsed_time(auto start_time, const char* label)
{
    std::chrono::duration<double, std::milli> duration = std::chrono::steady_clock::now() - start_time;
    std::cerr << label << ": " << duration.count() << "ms\n";
}

static
std::ostream& indent(std::ostream& out, uint16_t indent_level)
{
    for(uint16_t i = 0; i < indent_level; ++i) {
        out << "  ";
    }
    return out;
}

// TODO: how to do left vs. right associativity (currently we are essentially forcing right associativity
//  since we can only traverse the parse tree depth-first starting at the right)
static
void print_parse_tree(std::ostream& out, std::span<const Rule> rules, const SpanList<EarleyItem>& state_sets,
                      const Rule& rule, StateSetIterator curr_state_set, uint16_t indent_level = 0)
{
    for(auto comp_sym = rule.components.rbegin(); comp_sym < rule.components.rend(); ++comp_sym) {
        if(is_terminal(*comp_sym)) {
            /*
            Note: While there are one or more partially complete items in curr_state_set that correspond to our current
            subcomponent (those items' dots will be just before comp_sym), there is no point in searching
            for them since we already know the parent item, our position in it, the terminal symbol at that position,
            and how to get to the next relevant state set.
            */
            indent(out, indent_level) << *comp_sym << "\n";
            advance_from_terminal(curr_state_set);
        } else {
            auto curr_item = find_completed_item(rules, curr_state_set->begin(), curr_state_set->end(), *comp_sym);
            assert(curr_item != curr_state_set->end());
            indent(out, indent_level); print_item(out, rules, *curr_item) << "\n";
            print_parse_tree(out, rules, state_sets, rules[curr_item->rule_idx], curr_state_set, indent_level + 1);
            {
                auto alt_item = find_completed_item(rules, curr_item + 1, curr_state_set->end(), *comp_sym);
                while(alt_item != curr_state_set->end()) {
                    indent(out, indent_level) << "Alternative: "; print_item(out, rules, *alt_item) << "\n";
                    alt_item = find_completed_item(rules, alt_item + 1, curr_state_set->end(), *comp_sym);
                }
            }
            advance_from_nonterminal(state_sets, curr_state_set, curr_item);
        }
    }
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
    auto start_time = std::chrono::steady_clock::now();
    auto state_sets = parse(rules, start_symbol, input);
    print_elapsed_time(start_time, "Recognizer time");

    std::cerr << "\nState sets after parsing terminates:\n";
    uint32_t state_set_num = 0;
    for(const auto& state_set : state_sets) {
        std::cerr << "S(" << state_set_num++ << "):\n";
        print_state_set(std::cerr, rules, state_set) << "\n";
    }

    auto full_parse = find_full_parse(rules, start_symbol, state_sets, input);
    if(!full_parse) {
        std::cerr << "Error: parse failed\n";
        return 1;
    }
    const auto& full_parse_rule = rules[full_parse.item->rule_idx];
    std::cerr << "Full parse: "; print_item(std::cerr, rules, *full_parse.item) << "\n";

    std::cerr << "\nParse tree:\n";
    print_parse_tree(std::cerr, rules, state_sets, full_parse_rule, full_parse.state_set);
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
std::ostream& print_state_set(std::ostream& out, std::span<const Rule> rules, StateSet state_set)
{
    out << "{\n";
    for(const auto& item : state_set) {
        out << "  ";
        print_item(out, rules, item) << "\n";
    }
    return out << "}";
}