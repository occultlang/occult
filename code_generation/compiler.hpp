#pragma once
#include "code_gen.hpp"
#include "../parser/parser.hpp"
#include "../lexer/finder.hpp"

namespace occultlang
{
    class compiler
    {
        bool debug;
        std::string source_original;
    public:
        compiler(const std::string& source, bool debug) : source_original(source), debug(debug) {}
        std::pair<std::string, bool> compile();
    };
} // occultlang