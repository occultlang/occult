#pragma once
#include "code_gen.hpp"
#include "../lexer/convert_readable.hpp"
#include "../parser/parser.hpp"

namespace occultlang
{
    class compiler
    {
        bool debug;
        std::string source_original;
    public:
        compiler(const std::string& source, bool debug) : source_original(source), debug(debug) {}
        std::string compile();
    };
} // occultlang