#include "parser/parser.hpp"
#include <fstream>
#include <sstream>
#include "code_generation/code_gen.hpp"
#include <iostream>
#include <cstring> // for strcmp

extern "C" {
    #include "libtcc.h"
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::cerr << "usage: " << argv[0] << " <input_file>" << std::endl;
        return 1;
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

    std::cout << source_original << std::endl;

    occultlang::parser parser{ source_original };
    
    auto ast = parser.parse();

    //occultlang::ast::visualize(ast);

    //std::cout << std::endl;

    occultlang::code_gen code_gen;

    auto generated = code_gen.compile<occultlang::ast>(ast, true, occultlang::debug_level::none);

    std::cout << std::endl << generated << std::endl << std::endl;

    TCCState* tcc = tcc_new();

    if (tcc) {
        std::cout << "tcc state: " << tcc << std::endl;
    } else {
        std::cerr << "failed to create tcc state" << std::endl;
        return 1;
    }

    tcc_add_include_path(tcc, "./");
    std::cout << "added current dir to include path" << std::endl;

    tcc_add_library_path(tcc, "./");
    std::cout << "added current dir to library path" << std::endl;

    tcc_add_include_path(tcc, "/usr/include");
    std::cout << "added system include path" << std::endl;

    tcc_add_library_path(tcc, "/usr/lib");
    std::cout << "added system library path" << std::endl;

    tcc_set_options(tcc, "-g -nostdlib");
    std::cout << "tcc options: '-g -nostdlib'" << std::endl;

    tcc_set_output_type(tcc, TCC_OUTPUT_MEMORY);
    std::cout << "set output type to memory" << std::endl;

    if (tcc_compile_string(tcc, generated.c_str()) == -1) {
        std::cerr << "failed to compile code" << std::endl;
        return 1;
    }

    std::cout << "compiled using tcc" << std::endl;

    int result = tcc_run(tcc, 0, 0);
    std::cout << "result: " << result << std::endl;

    tcc_delete(tcc);

    return 0;
}
