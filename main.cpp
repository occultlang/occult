#include <iostream>
#include "code_generation/compiler.hpp"
#include "jit/tinycc_jit.hpp"
#include "lexer/finder.hpp"

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

    occultlang::finder finder;
    
    source_original = finder.match_and_replace_all_array(source_original, "array<generic>");

    source_original = finder.match_and_replace_all(source_original, "null", "NULL");

    source_original = finder.match_and_replace_casts(source_original);

    std::cout << source_original << std::endl;

    occultlang::compiler compiler{source_original, debug};

    std::string compiled = compiler.compile();

    occultlang::tinycc_jit jit{compiled};

    if (aot) {
        jit.run_aot();
    } else {
        jit.run();
    }

    return 0;
}