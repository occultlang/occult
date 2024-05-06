#include <iostream>
#include "code_generation/compiler.hpp"
#include "jit/tinycc_jit.hpp"
#include "lexer/finder.hpp"
#include "static_analyzer/static_analyzer.hpp"
#include <thread>

bool errors = false;

int main(int argc, char *argv[]) {
    bool debug = false;
    bool aot = false;

    if (argc < 2 || argc > 5) {
        std::cerr << "usage: " << argv[0] << " [-debug] <input_file> [-o <filename>]" << std::endl;
        return 1;
    }

    std::string output_file = "a.out";
    std::string input_file;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-o") {
            aot = true;
            if (i + 1 < argc) { 
                output_file = argv[++i]; 
            }
        } else if (arg == "-debug") {
            debug = true;
        } else {
            input_file = arg;
        }
    }

    if (input_file.empty()) {
        std::cerr << "No input file provided" << std::endl;
        return 1;
    }

    std::string source_original;
    std::ifstream file(input_file);

    if (!file.is_open()) {
        std::cerr << "unable to open file: " << input_file << std::endl;
        return 1;
    }

    std::stringstream ss;
    std::string line;

    while (getline(file, line)) {
        ss << line << "\n";
    }

    source_original = ss.str();
    file.close();

    occultlang::static_analyzer analyzer(source_original);

    analyzer.analyze();

    errors = !analyzer.get_error_list().empty();
    
    if (!errors) 
    {
        occultlang::finder finder;

        source_original = finder.match_and_replace_casts(source_original);

        source_original = finder.match_and_replace_all_array(source_original, "array<generic>");

        source_original = finder.match_and_replace_all(source_original, "null", "NULL");

        //std::cout << source_original << std::endl;

        occultlang::compiler compiler{source_original, debug};

        std::string compiled = compiler.compile();

        occultlang::tinycc_jit jit{compiled, output_file};

        if (aot) {
            jit.run_aot();
        } else {
            jit.run();
        }

        return 0;
    }
    else 
    {
        std::cout << analyzer.get_error_list();
        return 1;
    }
}