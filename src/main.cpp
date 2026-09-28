#include <chrono>
#include <cmath>
#include <csetjmp>
#include <csignal>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include "backend/codegen/ir_gen.hpp"
#include "backend/codegen/x86_64_codegen.hpp"
#include "code_analysis/linter.hpp"
#include "lexer/lexer.hpp"
#include "parser/cst.hpp"
#include "parser/parser.hpp"
#ifdef __linux
#include <sys/stat.h>
#include "backend/codegen/ir_lifter.hpp"
#include "backend/codegen/x86_64_assembler.hpp"
#include "backend/linker/linker.hpp"

static jmp_buf jit_jmp_buf;
static volatile sig_atomic_t jit_signal_caught = 0;

static void jit_signal_handler(int sig) {
    jit_signal_caught = sig;
    longjmp(jit_jmp_buf, 1);
}
#elif _WIN64
#include "backend/linker/pe_header.hpp"
#endif

void display_help() {
    std::cout << "Usage: occultc [options] <source.occ>\n"
              << "Info: Occult defaults to its JIT mode.\n"
              << "Options:\n"
              << "  -t,   --time              Show compilation time per stage\n"
              << "  -d,   --debug             Enable debug mode (implies --time)\n"
              << "  -o,   --output <file>     Output native binary\n"
              << "        --abi <sysv|win64>  Calling convention (default: host)\n"
              << "  -h,   --help              Show this message\n";
}

int main(int argc, char* argv[]) {
    std::string input_file;
    std::string source_original;

    bool debug = false;
    bool verbose = false;
    bool showtime = false;
    bool jit = true; // we will default to JIT

    std::string filenameout;
    occult::x86_64::call_abi abi = occult::x86_64::host_abi();

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
        else if (arg == "--abi" && i + 1 < argc) {
            const std::string v = argv[++i];
            if (v == "win64") abi = occult::x86_64::call_abi::win64;
            else if (v == "sysv") abi = occult::x86_64::call_abi::sysv;
            else { std::cout << "Unknown ABI: " << v << " (expected sysv or win64)\n"; return 1; }
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

    /*start = std::chrono::high_resolution_clock::now();
    occult::ir_lifter ir_lifter(ir); // lift to register ir
    auto reg_ir = ir_lifter.lift();
    end = std::chrono::high_resolution_clock::now();
    duration = end - start;
    if (showtime) {
        std::cout << GREEN << "[OCCULTC] Completed lifting IR \033[0m" << duration.count() << "ms\n";
    }
    if (debug) {
        occult::ir_lifter::visualize_register_ir(reg_ir);
    }*/

    start = std::chrono::high_resolution_clock::now();
    occult::x86_64::codegen_v2 codegen_v2(ir, ir_structs, debug, abi, ir_gen.program_globals);

    try {
        codegen_v2.compile(jit);
    }
    catch (const std::exception& e) {
        std::cerr << RED << "[OCCULTC] Code generation failed: " << RESET << e.what() << std::endl;
        return 1;
    }
    end = std::chrono::high_resolution_clock::now();
    duration = end - start;
    if (showtime) {
        std::cout << GREEN << "[OCCULTC] Completed converting IR to machine code \033[0m" << duration.count() << "ms\n";
    }

    /*if (debug && jit) {
      for (const auto& pair : jit_runtime.function_map) {
        std::cout << pair.first << std::endl;
        //std::cout << "0x" << std::hex <<
    //reinterpret_cast<std::int64_t>(&pair.second) << std::dec << std::endl;
      }
    }*/

#ifdef __linux
    if (jit) {
        if (auto it = codegen_v2.function_locs.find("main"); it != codegen_v2.function_locs.end()) {
            start = std::chrono::high_resolution_clock::now();

            struct sigaction sa{}, old_sigsegv{}, old_sigabrt{}, old_sigfpe{}, old_sigbus{};
            sa.sa_handler = jit_signal_handler;
            sa.sa_flags = 0;
            sigemptyset(&sa.sa_mask);
            sigaction(SIGSEGV, &sa, &old_sigsegv);
            sigaction(SIGABRT, &sa, &old_sigabrt);
            sigaction(SIGFPE, &sa, &old_sigfpe);
            sigaction(SIGBUS, &sa, &old_sigbus);

            std::int64_t res = 0;
            if (setjmp(jit_jmp_buf) == 0) {
                if (codegen_v2.function_locs.count("__global_init")) {
                    codegen_v2.get_function<std::int64_t(*)()>("__global_init")();
                }
                auto main_fn = codegen_v2.get_function<std::int64_t(*)()>("main");
                res = main_fn();
            }
            else {
                sigaction(SIGSEGV, &old_sigsegv, nullptr);
                sigaction(SIGABRT, &old_sigabrt, nullptr);
                sigaction(SIGFPE, &old_sigfpe, nullptr);
                sigaction(SIGBUS, &old_sigbus, nullptr);
                std::cerr << RED << "[OCCULTC] JIT execution crashed (signal " << jit_signal_caught << ")" << RESET << std::endl;
                return 1;
            }

            sigaction(SIGSEGV, &old_sigsegv, nullptr);
            sigaction(SIGABRT, &old_sigabrt, nullptr);
            sigaction(SIGFPE, &old_sigfpe, nullptr);
            sigaction(SIGBUS, &old_sigbus, nullptr);

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
#else
    if (jit) {
        if (auto it = codegen_v2.function_locs.find("main"); it != codegen_v2.function_locs.end()) {
            start = std::chrono::high_resolution_clock::now();

            if (codegen_v2.function_locs.count("__global_init")) {
                codegen_v2.get_function<std::int64_t (*)()>("__global_init")();
            }
            std::int64_t res = codegen_v2.get_function<std::int64_t (*)()>("main")();

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
#endif
#ifdef __linux
    if (!jit) {
        occult::linker::link_blob(filenameout, codegen_v2.get_code(), codegen_v2.function_locs, codegen_v2.string_relocs, codegen_v2.string_literals, codegen_v2.global_relocs, codegen_v2.globals_size(), debug, showtime);

        chmod(filenameout.c_str(), S_IRWXU);
    }
#endif

    return 0;
}
