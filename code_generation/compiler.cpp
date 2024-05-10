#include "compiler.hpp"

namespace occultlang
{
    std::pair<std::string, bool> compiler::compile()
    {       
        finder finder;

        source_original = finder.match_and_replace_casts(source_original);

        source_original = finder.match_and_replace_all_array(source_original, "array<generic>");

        source_original = finder.match_and_replace_all(source_original, "null", "NULL");

        // std::cout << source_original << std::endl;

        occultlang::parser parser{ source_original };

        if (debug)
            occultlang::lexer::visualize(parser.get_tokens());

        auto ast = parser.parse();

        if (debug)
            occultlang::ast::visualize(ast);

        occultlang::code_gen code_gen{parser};

        auto generated = code_gen.generate<occultlang::ast>(ast, debug, occultlang::debug_level::all);

        if (debug)
            std::cout << std::endl << code_gen.func_defs + generated << std::endl << std::endl;

        if (code_gen.get_symbols().count("main") == 0) 
        {
            std::cerr << "\033[31mCompilation Error: No main function found\033[0m\n\033[94mNotice: this also happens if there is a symbol already existing!\033[0m" << std::endl;
            return std::make_pair("", false);
        }
        else 
            return std::make_pair(code_gen.lib + code_gen.func_defs + generated, true);
    }
} // occultlang
