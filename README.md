# Libearley

A C++ [Earley parser](https://en.wikipedia.org/wiki/Earley_parser) library.

## Features

- Includes a recognizer for matching grammar rules and functions for traversing the output to build a parse tree
- Includes functions for pretty-printing rules and Earley items
- Can be run on partial input
- Written as a generic library
    - Can parse any type conforming to the [std::ranges::input_range](https://en.cppreference.com/w/cpp/ranges/input_range) concept.
        - Works with `std::string`, `std::ifstream`, `std::span<CustomTokenType>`, etc.
    - Symbols used in the grammar can be any [regular type](https://en.cppreference.com/w/cpp/concepts/regular) that can be converted into an integer.
        - This includes enums, characters, or even user-created classes.

## Usage

### Dependencies

- A compiler that supports C++20
- Boost (tested with 1.82, but should works with any version that supports C++20 and includes
  the [Boost.STLInterfaces](https://www.boost.org/doc/libs/1_84_0/doc/html/stl_interfaces.html) library)

### Building

Since this is a template library, it is header only.

The `CMakeLists.txt` file in the root of this repository can be added to your project. Alternatively,
the files in the `lib` subdirectory can be added directly to your build system.

### Example

The core functionality of the library is provided in `earley.hpp`. `earley_print.hpp` includes functions for
printing some of the library's data structures to iostreams, which might be useful during development.

In order to build a parser, the library requires the user to provide the following parameters:

- A **token type**: This type represents one element of the input.
    - This is often a character type (since many parsers parse character strings), but it can be any other
      type, such as the type of tokens emitted by a lexer.
- A **symbol type**: This type represents the terminal and non-terminal symbols of the grammar.
    - Typically, an enum class type should be used.
        - **Note:** For enum class types, you must provide a special enum member named `Symbol_Count`. This
          will be used in the parser to determine the number of possible symbols.
        - For types that are not enum classes, a custom specialization of `earley::symbol_traits` must be
          provided for your symbol type.
    - A pair of helper functions with the signatures `bool is_terminal(Symbol)` and
      `bool matches_terminal(Symbol, Token)` must also be provided. These are used to check if a symbol is a terminal
      and if a given terminal symbol matches a given token, respectively.
- A **grammar**: This is a collection of parsing rules.
    - It must be a contiguous collection of `earley::Rule<Symbol>` objects (e.g. an array of `earley::Rule<Symbol>`)
    - The grammar rules must get wrapped in an `earley::RuleSet<Symbol>` object. This is what is passed to the parser.
    - Each rule consists of a "left-hand side", which is a non-terminal symbol, and a "right-hand side", which
      is a sequence of zero or more terminal and/or non-terminal symbols.
    - The parser assumes that all rules with the same left-hand side are adjacent to each other in the grammar.
- A **start symbol**: The goal of parsing is to the match the input to a rule that has this symbol as its left-hand
  side.

To parse input, call the function `earley::parse` and pass the above information as parameters. This function
will return a sequence of *state sets*, each of which holds the Earley items matched against the
input at each position. In other words, `state_sets[0]` represents the items considered at the
first element of the input, `state_sets[1]` at the second element, and so on.

Here is an example program that parses the given input and prints the rule that matched the full input (if any).
This code can also be found in `test/example.cpp`.

```cpp
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

    sum = sum "+" product;
    sum = sum "-" product;
    sum = product;
    product = product "/" factor;
    product = product "*" factor;
    product = factor;
    factor = "(" sum ")";
    factor = number;
    number = digit;
    number = digit number;
    digit = "0" | "1" | "2" | "3" | "4" | "5" | "6" | "7" | "8" | "9";

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
```

The output should be:

```
Full parse: Sum -> Sum '+' Product . (0)
```

## License

This library is licensed under the GPL-3.0 license. For more information,
see the LICENSE file in this directory