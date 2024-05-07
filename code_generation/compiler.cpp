#include "compiler.hpp"

namespace occultlang
{
    std::string compiler::compile()
    {       
        finder finder;

        source_original = finder.match_and_replace_casts(source_original);

        source_original = finder.match_and_replace_all_array(source_original, "array<generic>");

        source_original = finder.match_and_replace_all(source_original, "null", "NULL");

        std::cout << source_original << std::endl;

        occultlang::parser parser{ source_original };

        if (debug)
            occultlang::lexer::visualize(parser.get_tokens());

        auto ast = parser.parse();

        if (debug)
            occultlang::ast::visualize(ast);

        occultlang::code_gen code_gen;

        auto generated = code_gen.generate<occultlang::ast>(ast, debug, occultlang::debug_level::all);

        if (debug)
            std::cout << std::endl << code_gen.func_defs + generated << std::endl << std::endl;

        return code_gen.lib + code_gen.func_defs + generated;
    }
} // occultlang