#include <cstdint>
#include <iostream>
#include <span>
#include <string_view>
#include <cstddef>
#include "earley.hpp"
#include "earley_print.hpp"

enum class Symbol {
    /* Terminals */
    Plus, Minus, Mult, Div, LParen, RParen, Digit,
    /* Nonterminals */
    Number, Sum, Product, Factor,

    Symbol_Count
};

static constexpr
bool is_terminal(Symbol s) { return (uint8_t)s <= (uint8_t)Symbol::Digit; }

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
std::ostream& operator<<(std::ostream& out, Symbol s);

int main()
{
    /*
    This grammar represents the following EBNF grammar:
    Sum ::= Sum [+-] Product
    Sum ::= Product
    Product ::= Product [/*] Factor
    Product ::= Factor
    Factor ::= ( Sum )
    Factor ::= Number
    Number ::= Digit
    Number ::= Digit Number

    Note: this grammar is the same as the grammar used in the excellent article series by
    Loup Vaillant: https://loup-vaillant.fr/tutorials/earley-parsing/
    */
    using enum Symbol;
    static const earley::Rule<Symbol> rules[] = {
        { Sum,     { Sum, Plus, Product } },
        { Sum,     { Sum, Minus, Product } },
        { Sum,     { Product } },
        { Product, { Product, Mult, Factor } },
        { Product, { Product, Div, Factor } },
        { Product, { Factor } },
        { Factor,  { LParen, Sum, RParen } },
        { Factor,  { Number } },
        { Number,  { Digit } },
        { Number,  { Digit, Number } }
    };
    constexpr auto start_symbol = Sum;
    std::span<const earley::Rule<Symbol>> rules_view = rules;
    earley::RuleSet rule_set{rules_view};

    std::string_view input = "1+(8*9)";
    auto state_sets = parse<char>(rule_set, start_symbol, input);

    auto full_parse = earley::find_full_parse(rules_view, start_symbol, state_sets, input);
    if(!full_parse) {
        std::cerr << "Error: parse failed\n";
        return 1;
    }
    // Use one of the printing functions to print the rule that was used to completely parse the input
    std::cerr << "Full parse: "; earley::print_item(std::cerr, rules_view, *full_parse.item) << "\n";

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