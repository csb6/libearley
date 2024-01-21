#include <iostream>
#include <fstream>
#include <iterator>
#include <cassert>
#include <span>
#include <string>
#include <cstdint>
#include <cctype>
#include <ratio>
#include <chrono>
#include "earley_print.hpp"
#include "span_list.hpp"
#include "earley.hpp"

enum class Symbol : uint8_t {
    /* Terminals */
    Plus, Minus, Mult, Div, LParen, RParen, Digit,
    /* Nonterminals */
    Number, Sum, Product, Factor,

    Symbol_Count,
    First_Symbol = Plus,
    Last_Terminal = Digit
};

using Rule = earley::Rule<Symbol>;

static
std::ostream& operator<<(std::ostream&, Symbol);

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

static
void print_elapsed_time(auto start_time, const char* label)
{
    std::chrono::duration<double, std::milli> duration = std::chrono::steady_clock::now() - start_time;
    std::cerr << label << ": " << duration.count() << "ms\n";
}

//static
//std::ostream& indent(std::ostream& out, uint16_t indent_level)
//{
//    for(uint16_t i = 0; i < indent_level; ++i) {
//        out << "  ";
//    }
//    return out;
//}

// TODO: how to do left vs. right associativity (currently we are essentially forcing right associativity
//  since we can only traverse the parse tree depth-first starting at the right)
static
void print_parse_tree(std::ostream& out, std::span<const Rule> rules, const SpanList<earley::EarleyItem>& state_sets,
                      const Rule& rule, earley::StateSetIterator curr_state_set, uint16_t indent_level = 0)
{
    for(auto comp_sym = rule.components.rbegin(); comp_sym < rule.components.rend(); ++comp_sym) {
        if(is_terminal(*comp_sym)) {
            /*
            Note: While there are one or more partially complete items in curr_state_set that correspond to our current
            subcomponent (those items' dots will be just before comp_sym), there is no point in searching
            for them since we already know the parent item, our position in it, the terminal symbol at that position,
            and how to get to the next relevant state set.
            */
            //indent(out, indent_level) << *comp_sym << "\n";
            earley::advance_from_terminal(curr_state_set);
        } else {
            auto curr_item = earley::find_completed_item(rules, curr_state_set->begin(), curr_state_set->end(), *comp_sym);
            assert(curr_item != curr_state_set->end());
            //indent(out, indent_level); print_item(out, rules, *curr_item) << "\n";
            print_parse_tree(out, rules, state_sets, rules[curr_item->rule_idx], curr_state_set, indent_level + 1);
            {
                auto alt_item = earley::find_completed_item(rules, curr_item + 1, curr_state_set->end(), *comp_sym);
                while(alt_item != curr_state_set->end()) {
                    //indent(out, indent_level) << "Alternative: "; print_item(out, rules, *alt_item) << "\n";
                    alt_item = earley::find_completed_item(rules, alt_item + 1, curr_state_set->end(), *comp_sym);
                }
            }
            earley::advance_from_nonterminal(state_sets, curr_state_set, curr_item);
        }
    }
}


int main(int argc, char** argv)
{
    if(argc != 2) {
        std::cerr << "Usage: " << argv[0] << " inputFile\n";
        return 1;
    }

    using enum Symbol;
    static const Rule rules[] = {
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
    std::span<const Rule> rules_view = rules;
    earley::RuleSet rule_set{rules_view};

    std::ifstream input_file{argv[1]};
    if(!input_file) {
        std::cerr << "Error: failed to open input file: '" << argv[1] << "'\n";
        return 1;
    }
    std::string input{std::istream_iterator<char>{input_file}, std::istream_iterator<char>{}};
    std::cerr << "Input length: " << input.size() << " bytes\n";

    auto start_time = std::chrono::steady_clock::now();
    auto state_sets = parse<char>(rule_set, start_symbol, input);
    print_elapsed_time(start_time, "Recognizer time");

    // std::cerr << "\nState sets after parsing terminates:\n";
    // uint32_t state_set_num = 0;
    // for(const auto& state_set : state_sets) {
    //     std::cerr << "S(" << state_set_num++ << "):\n";
    //     print_state_set(std::cerr, rules, state_set) << "\n";
    // }

    auto full_parse = earley::find_full_parse(rules_view, start_symbol, state_sets, input);
    if(!full_parse) {
        std::cerr << "Error: parse failed\n";
        return 1;
    }
    const auto& full_parse_rule = rules[full_parse.item->rule_idx];
    std::cerr << "Full parse: "; earley::print_item(std::cerr, rules_view, *full_parse.item) << "\n";

    std::cerr << "\nTraverse parse tree:\n";
    start_time = std::chrono::steady_clock::now();
    print_parse_tree(std::cerr, rules, state_sets, full_parse_rule, full_parse.state_set);
    print_elapsed_time(start_time, "Parse traversal time");

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
        case Symbol_Count:
            break;
    }
    return out;
}