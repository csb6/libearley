#include <cstdint>
#include <string_view>
#include <iostream>
#include "earley.hpp"
#include "earley_print.hpp"

enum class Symbol : uint8_t {
    A, B,
    Symbol_Count
};

static
std::ostream& operator<<(std::ostream& out, Symbol s)
{
    using enum Symbol;
    switch(s) {
        case A:
            out << "A";
            break;
        case B: out << "B";
            break;
        default:
            break;
    }
    return out;
}

static constexpr
bool is_terminal(Symbol) noexcept { return false; }

static constexpr
bool matches_terminal(Symbol, char) noexcept { return false; }

int main()
{
    using enum Symbol;
    static const earley::Rule<Symbol> rules[] = {
        { A, {} },
        { A, { B } },
        { B, { A } }
    };
    constexpr auto start_symbol = A;
    std::span<const earley::Rule<Symbol>> rules_view = rules;
    earley::RuleSet<Symbol> rule_set{rules_view};

    std::string_view input = "";
    auto state_sets = earley::parse<char>(rule_set, start_symbol, input);

    // Expected:
    //  A -> . (0)
    //  A -> . B (0)
    //  B -> . A (0)
    //  A -> B . (0) (advanced during prediction step due to B being nullable)
    //  B -> A . (0) (advanced during prediction step due to A being nullable)
    for(const auto state_set : state_sets) {
        earley::print_state_set(std::cerr, rules_view, state_set) << "\n";
    }

    return 0;
}