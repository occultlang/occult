#include "compiler.hpp"

namespace occultlang
{
    std::string compiler::compile()
    {       
        occultlang::parser parser{ source_original };

        if (debug)
            occultlang::lexer::visualize(parser.get_tokens());

        auto ast = parser.parse();

        if (debug)
            occultlang::ast::visualize(ast);

        occultlang::code_gen code_gen;

        auto generated = code_gen.generate<occultlang::ast>(ast, debug, occultlang::debug_level::all);

        if (debug)
            std::cout << std::endl << generated << std::endl << std::endl;

        return code_gen.lib + generated;
    }
} // occultlang