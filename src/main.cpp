#include <chrono>
#include <cmath>
#include <csetjmp>
#include <csignal>
#include <cstdlib>
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
              << "  -h,   --help              Show this message\n";
}

OCCULT_FUNC_DECL(std::int64_t, alloc, (std::int64_t sz), std::int64_t) {
    if (sz <= 0 || static_cast<std::uint64_t>(sz) > SIZE_MAX) {
        return 0;
    }
    auto ptr = malloc(static_cast<std::size_t>(sz));
    return reinterpret_cast<std::int64_t>(ptr);
}

OCCULT_FUNC_DECL(std::int64_t, del, (std::int64_t* ptr), std::int64_t) {
    if (ptr == nullptr) {
        return 0;
    }
    free(ptr);
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
    /*occult::function_registry::register_function_to_ir<&print_string>(ir);
    occult::function_registry::register_function_to_ir<&print_integer>(ir);
    occult::function_registry::register_function_to_ir<&print_newline>(ir);
    occult::function_registry::register_function_to_ir<&print_char>(ir);*/

    start = std::chrono::high_resolution_clock::now();
    occult::x86_64::codegen jit_runtime(ir, ir_structs, debug);

    occult::function_registry::register_function_to_codegen<&alloc>(jit_runtime);
    occult::function_registry::register_function_to_codegen<&del>(jit_runtime);
    /* occult::function_registry::register_function_to_codegen<&print_string>(jit_runtime);
     occult::function_registry::register_function_to_codegen<&print_integer>(jit_runtime);
     occult::function_registry::register_function_to_codegen<&print_newline>(jit_runtime);
     occult::function_registry::register_function_to_codegen<&print_char>(jit_runtime);*/

    try {
        jit_runtime.compile(jit);
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

    /*occult::x86_64::assembler assembler(R"(
        push rbp
        mov	rbp, rsp
        mov r11, rdi
        mov r12, rsi
        mov rax, 1
        mov rdi, 1
        mov rsi, r11
        mov rdx, r12
        syscall
        mov	rsp,rbp
        pop	rbp
        mov rax, 0
        ret
    )", true);
    assembler.assemble();*/

    /*if (debug && jit) {
      for (const auto& pair : jit_runtime.function_map) {
        std::cout << pair.first << std::endl;
        std::cout << "0x" << std::hex <<
    reinterpret_cast<std::int64_t>(&pair.second) << std::dec << std::endl;
      }
    }*/

#ifdef __linux
    if (jit) {
        if (auto it = jit_runtime.function_map.find("main"); it != jit_runtime.function_map.end()) {
            start = std::chrono::high_resolution_clock::now();

            // Install signal handlers to catch JIT crashes
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
                res = reinterpret_cast<std::int64_t (*)()>(it->second)();
            }
            else {
                // Restore default signal handlers
                sigaction(SIGSEGV, &old_sigsegv, nullptr);
                sigaction(SIGABRT, &old_sigabrt, nullptr);
                sigaction(SIGFPE, &old_sigfpe, nullptr);
                sigaction(SIGBUS, &old_sigbus, nullptr);
                std::cerr << RED << "[OCCULTC] JIT execution crashed (signal " << jit_signal_caught << ")" << RESET << std::endl;
                return 1;
            }

            // Restore default signal handlers
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
        if (auto it = jit_runtime.function_map.find("main"); it != jit_runtime.function_map.end()) {
            start = std::chrono::high_resolution_clock::now();

            std::int64_t res = reinterpret_cast<std::int64_t (*)()>(it->second)();

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
    else if (!jit) {
        occult::linker::link_and_create_binary(filenameout, jit_runtime.function_map, jit_runtime.function_raw_code_map, jit_runtime.string_literals, debug, showtime);

        chmod(filenameout.c_str(), S_IRWXU);
    }
#endif

    return 0;
}
