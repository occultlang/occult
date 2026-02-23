#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include "backend/codegen/function_registry.hpp"
#include "backend/codegen/ir_gen.hpp"
#include "backend/codegen/x86_64_codegen.hpp"
#include "code_analysis/linter.hpp"
#include "lexer/lexer.hpp"
#include "parser/cst.hpp"
#include "parser/parser.hpp"
#ifdef __linux
#include <sys/stat.h>
#include "backend/linker/linker.hpp"
#elif _WIN64
#include "backend/linker/pe_header.hpp"
#endif

#include <cstdlib>

void display_help() {
    std::cout << "Usage: occultc [options] <source.occ>\n"
              << "Info: Occult defaults to its JIT mode.\n"
              << "Options:\n"
              << "  -t,   --time              Show compilation time per stage\n"
              << "  -d,   --debug             Enable debug mode (implies --time)\n"
              << "  -o,   --output <file>     Output native binary\n"
              << "  -h,   --help              Show this message\n"
              << "  -gc,  --gc                Enable the Garbage Collector\n";
}

OCCULT_FUNC_DECL(std::int64_t, alloc, (std::int64_t sz), std::int64_t) {
    auto ptr = malloc(sz);
    return reinterpret_cast<std::int64_t>(ptr);
}

OCCULT_FUNC_DECL(std::int64_t, del, (std::int64_t* ptr), std::int64_t) {
    free(ptr);
    return 0;
}

OCCULT_FUNC_DECL(std::int64_t, print_string, (std::int64_t str), std::int64_t) {
    if (str == 0) {
        return 0;
    }
    const char* ptr = reinterpret_cast<const char*>(str);
    std::size_t len = 0;
    while (ptr[len] != '\0') {
        ++len;
    }
    std::cout.write(ptr, static_cast<std::streamsize>(len));
    std::cout.flush();
    return static_cast<std::int64_t>(len);
}

OCCULT_FUNC_DECL(std::int64_t, print_integer, (std::int64_t num), std::int64_t) {
    std::string s = std::to_string(num);
    std::cout << s;
    std::cout.flush();
    return 0;
}

OCCULT_FUNC_DECL(std::int64_t, print_newline, (), std::int64_t) {
    std::cout.put('\n');
    std::cout.flush();
    return 0;
}

int main(int argc, char* argv[]) {
    std::string input_file;
    std::string source_original;

    bool debug = false;
    bool verbose = false;
    bool showtime = false;
    bool jit = true; // we will default to JIT

    std::string filenameout;

    for (int i = 1; i < argc; ++i) {
        if (std::string arg = argv[i]; arg == "-d" || arg == "--debug") {
            debug = true;
            verbose = true;
            showtime = true;
        }
        else if (arg == "-t" || arg == "--time") {
            showtime = true;
        }
        else if (arg == "-o" || arg == "--output") {
            jit = false;

            if (i + 1 < argc) {
                jit = false;
                filenameout = argv[++i];
            }
            else {
                filenameout = "a.out";
            }
        }
        else if (arg == "-h" || arg == "--help") {
            display_help();

            return 0;
        }
        else {
            input_file = arg;
        }
    }

    std::ifstream file(input_file);
    std::stringstream buffer;
    buffer << file.rdbuf();
    source_original = buffer.str();

    if (input_file.empty()) {
        std::cout << RED << "[-] No input file specified" << RESET << std::endl;
        display_help();

        return 0;
    }

    auto start = std::chrono::high_resolution_clock::now();
    occult::lexer lexer(source_original);
    auto tokens = lexer.analyze();
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration = end - start;
    if (showtime) {
        std::cout << GREEN << "[OCCULTC] Completed lexical analysis \033[0m" << duration.count() << "ms\n";
    }
    if (debug && verbose) {
        lexer.visualize();
    }

    start = std::chrono::high_resolution_clock::now();
    occult::parser parser(tokens, input_file, source_original);
    auto cst = parser.parse();
    end = std::chrono::high_resolution_clock::now();
    duration = end - start;
    if (parser.get_state() == occult::parser::state::failed) {
        std::cout << RED << "[OCCULTC] Parsing failed with " << parser.get_error_count() << " error(s)" << RESET << std::endl;
        return 1;
    }

    if (showtime) {
        std::cout << GREEN << "[OCCULTC] Completed parsing \033[0m" << duration.count() << "ms\n";
    }
    if (debug && verbose) {
        cst->visualize();
    }

    occult::linter linter(cst.get(), debug);
    const bool lint_ok = linter.analyze();
    for (const auto& err : linter.get_errors()) {
        const char* prefix = (err.level == occult::lint_error::severity::error) ? RED "[LINT ERROR] " RESET : YELLOW "[LINT WARN]  " RESET;
        std::cout << prefix << err.message << "\n";
    }
    if (!lint_ok) {
        std::cout << RED << "[OCCULTC] Linting failed \u2014 " << linter.get_errors().size() << " error(s)" << RESET << "\n";
        return 1;
    }

    start = std::chrono::high_resolution_clock::now();
    occult::ir_gen ir_gen(cst.get(), parser.get_custom_type_map(), debug);
    auto ir_structs = ir_gen.lower_structs();
    auto ir = ir_gen.lower_functions();
    end = std::chrono::high_resolution_clock::now();
    duration = end - start;
    if (showtime) {
        std::cout << GREEN << "[OCCULTC] Completed generating IR \033[0m" << duration.count() << "ms\n";
    }
    if (debug) {
        occult::ir_gen::visualize_structs(ir_structs);
        occult::ir_gen::visualize_stack_ir(ir);
    }


    occult::function_registry::register_function_to_ir<&alloc>(ir);
    occult::function_registry::register_function_to_ir<&del>(ir);
    occult::function_registry::register_function_to_ir<&print_string>(ir);
    occult::function_registry::register_function_to_ir<&print_integer>(ir);
    occult::function_registry::register_function_to_ir<&print_newline>(ir);

    start = std::chrono::high_resolution_clock::now();
    occult::x86_64::codegen jit_runtime(ir, ir_structs, debug);

    occult::function_registry::register_function_to_codegen<&alloc>(jit_runtime);
    occult::function_registry::register_function_to_codegen<&del>(jit_runtime);
    occult::function_registry::register_function_to_codegen<&print_string>(jit_runtime);
    occult::function_registry::register_function_to_codegen<&print_integer>(jit_runtime);
    occult::function_registry::register_function_to_codegen<&print_newline>(jit_runtime);

    jit_runtime.compile(jit);
    end = std::chrono::high_resolution_clock::now();
    duration = end - start;
    if (showtime) {
        std::cout << GREEN << "[OCCULTC] Completed converting IR to machine code \033[0m" << duration.count() << "ms\n";
    }
    /*if (debug && jit) {
      for (const auto& pair : jit_runtime.function_map) {
        std::cout << pair.first << std::endl;
        std::cout << "0x" << std::hex <<
    reinterpret_cast<std::int64_t>(&pair.second) << std::dec << std::endl;
      }
    }*/

    if (jit) {
        if (auto it = jit_runtime.function_map.find("main"); it != jit_runtime.function_map.end()) {
            start = std::chrono::high_resolution_clock::now();

            auto res = reinterpret_cast<std::int64_t (*)()>(it->second)();

            end = std::chrono::high_resolution_clock::now();
            duration = end - start;

            if (debug) {
                std::cout << "Main returned: " << res << std::endl;
            }

            if (showtime) {
                std::cout << GREEN << "[OCCULTC] Completed executing jit code " << RESET << duration.count() << "ms\n";
            }
        }
        else {
            std::cerr << "Main function not found!" << std::endl;
        }
    }

#ifdef __linux
    else if (!jit) {
        occult::linker::link_and_create_binary(filenameout, jit_runtime.function_map, jit_runtime.function_raw_code_map, jit_runtime.string_literals, debug, showtime);

        chmod(filenameout.c_str(), S_IRWXU);
    }
#endif

    return 0;
}
