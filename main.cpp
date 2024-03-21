#include "parser/parser.hpp"
#include <fstream>
#include <sstream>
#include "code_generation/code_gen.hpp"
#include <iostream>

extern "C" {
    #include "libtcc.h"
}

int main(int argc, char *argv[]) {
    bool debug = false;
    bool aot = false;

    if (argc < 2 || argc > 4) {
        std::cerr << "usage: " << argv[0] << " <input_file> [-aot] [-dbg]" << std::endl;
        return 1;
    }

     for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-aot") {
            aot = true;
        } else if (arg == "-dbg") {
            debug = true;
        }
    }

    std::string source_original;
    std::ifstream file(argv[1]);

    if (!file.is_open()) {
        std::cerr << "unable to open file: " << argv[1] << std::endl;
        return 1;
    }

    std::stringstream ss;
    std::string line;

    while (getline(file, line)) {
        ss << line << "\n";
    }

    source_original = ss.str();
    file.close();

    //std::cout << source_original << std::endl;

    occultlang::parser parser{ source_original };

    if (debug)
        occultlang::lexer::visualize(parser.get_tokens());

    auto ast = parser.parse();

    if (debug)
        occultlang::ast::visualize(ast);

    // std::cout << std::endl;

    occultlang::code_gen code_gen;

    auto generated = code_gen.compile<occultlang::ast>(ast, debug, occultlang::debug_level::all);

    if (debug)
        std::cout << std::endl << generated << std::endl << std::endl;

    TCCState* tcc = tcc_new();

    if (!tcc) {
        std::cerr << "failed to create tcc state" << std::endl;
        return 1;
    }

    tcc_add_library_path(tcc, "./");

    tcc_set_options(tcc, "-g -w");

    if (aot) {
        tcc_set_output_type(tcc, TCC_OUTPUT_EXE);
    }
    else {
        tcc_set_output_type(tcc, TCC_OUTPUT_MEMORY);
    }

    if (tcc_compile_string(tcc, std::string(code_gen.lib + generated).c_str()) == -1) {
        std::cerr << "failed to compile code" << std::endl;
        return 1;
    }

    if (aot) {
        tcc_output_file(tcc, "occult_out");
    }
    else {
        tcc_run(tcc, 0, 0);
    }
    
    tcc_delete(tcc);

    return 0;
}
