#include <algorithm>
#include <chrono>
#include <cmath>
#include <csetjmp>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>
#include "backend/codegen/ir_gen.hpp"
#include "backend/codegen/x86_64_codegen.hpp"
#include "code_analysis/linter.hpp"
#include "lexer/lexer.hpp"
#include "parser/cst.hpp"
#include "parser/error_builtin.hpp"
#include "parser/parser.hpp"
#include "parser/parser_maps.hpp"
#include "backend/codegen/ir_lifter.hpp"
#include "backend/codegen/x86_64_assembler.hpp"
#include "backend/linker/coff_object.hpp"
#include "backend/linker/elf_object.hpp"
#include "backend/linker/linker.hpp"

// a crash in JIT'd code is caught rather than taking the compiler with it
static jmp_buf jit_jmp_buf;
static volatile sig_atomic_t jit_signal_caught = 0;

#ifdef __linux
#include <link.h>
#include <sys/stat.h>

static std::vector<std::string> library_candidates(const std::string& name) {
    if (name.find('/') != std::string::npos || name.find(".so") != std::string::npos) {
        return {name};
    }
    // link "libz" and link "z" are one library, as -lz is
    if (name.rfind("lib", 0) == 0 && name.size() > 3) {
        return library_candidates(name.substr(3));
    }

    std::vector<std::string> dirs;
    if (const char* p = std::getenv("LD_LIBRARY_PATH")) {
        std::stringstream ss(p);
        for (std::string d; std::getline(ss, d, ':');) {
            if (!d.empty()) {
                dirs.push_back(d);
            }
        }
    }
    for (const char* d : {"/lib/x86_64-linux-gnu", "/usr/lib/x86_64-linux-gnu", "/lib64",
                          "/usr/lib64", "/usr/local/lib", "/lib", "/usr/lib"}) {
        dirs.emplace_back(d);
    }

    const std::string stem = "lib" + name + ".so.";
    std::vector<std::string> sonames;
    for (const auto& d : dirs) {
        std::error_code ec;
        std::filesystem::directory_iterator it(d, ec);
        if (ec) {
            continue;
        }
        for (const auto& entry : it) {
            const std::string f = entry.path().filename().string();
            if (f.rfind(stem, 0) == 0) {
                sonames.push_back(entry.path().string());
            }
        }
    }
    std::sort(sonames.begin(), sonames.end(), std::greater<>{});

    std::vector<std::string> out{"lib" + name + ".so"};
    out.insert(out.end(), sonames.begin(), sonames.end());

    return out;
}

static void jit_signal_handler(int sig) {
    jit_signal_caught = sig;
    longjmp(jit_jmp_buf, 1);
}
#endif
#ifdef _WIN64
static LONG CALLBACK jit_exception_filter(EXCEPTION_POINTERS* info) {
    jit_signal_caught = static_cast<int>(info->ExceptionRecord->ExceptionCode);
    longjmp(jit_jmp_buf, 1);
}

static std::vector<std::string> library_candidates(const std::string& name) {
    if (name.find_first_of("/\\.") != std::string::npos) {
        return {name};
    }
    if (name.rfind("lib", 0) == 0 && name.size() > 3) {
        return {name, name.substr(3)};
    }
    return {name, "lib" + name};
}
#endif

static void* open_library(const std::string& lib, std::string& why) {
#ifdef __linux__
    // RTLD_GLOBAL so that dlsym(RTLD_DEFAULT, ...) can see it as well
    for (const auto& candidate : library_candidates(lib)) {
        if (void* h = dlopen(candidate.c_str(), RTLD_NOW | RTLD_GLOBAL)) {
            return h;
        }
        why = dlerror();
    }
#elif defined(_WIN64)
    for (const auto& candidate : library_candidates(lib)) {
        if (HMODULE h = LoadLibraryA(candidate.c_str())) {
            return reinterpret_cast<void*>(h);
        }
        why = "error " + std::to_string(GetLastError());
    }
#else
    why = "this host has no loader";
#endif
    return nullptr;
}

// what cc needs for a library: -lz for a name, the file for a path
static std::string cc_library_flag(const std::string& lib) {
    if (lib.find('/') != std::string::npos || lib.find(".so") != std::string::npos ||
        lib.find(".a") != std::string::npos) {
        return lib;
    }
    if (lib.rfind("lib", 0) == 0 && lib.size() > 3) {
        return "-l" + lib.substr(3);
    }
    return "-l" + lib;
}

// a name in a library this process opened, or null
static void* lookup_symbol(void* handle, const std::string& name) {
#ifdef __linux__
    return dlsym(handle, name.c_str());
#elif defined(_WIN64)
    return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(handle), name.c_str()));
#else
    (void)handle;
    (void)name;
    return nullptr;
#endif
}

static std::string library_file_name(const std::string& lib, const std::unordered_map<std::string, void*>& handles,
                                     bool can_load, bool pe) {
    if (auto h = handles.find(lib); can_load && h != handles.end()) {
#ifdef __linux__
        struct link_map* lm = nullptr;
        if (dlinfo(h->second, RTLD_DI_LINKMAP, &lm) == 0 && lm != nullptr) {
            std::uintptr_t strtab = 0;
            const ElfW(Dyn)* soname = nullptr;
            for (const ElfW(Dyn)* d = lm->l_ld; d->d_tag != DT_NULL; ++d) {
                if (d->d_tag == DT_STRTAB) {
                    strtab = d->d_un.d_ptr;
                }
                else if (d->d_tag == DT_SONAME) {
                    soname = d;
                }
            }
            if (strtab != 0 && strtab < lm->l_addr) {
                strtab += lm->l_addr;
            }
            if (soname != nullptr && strtab != 0) {
                return reinterpret_cast<const char*>(strtab + soname->d_un.d_val);
            }
            if (lm->l_name != nullptr && *lm->l_name != '\0') {
                return std::filesystem::path(lm->l_name).filename().string();
            }
        }
#elif defined(_WIN64)
        char path[MAX_PATH];
        const DWORD n = GetModuleFileNameA(static_cast<HMODULE>(h->second), path, MAX_PATH);
        if (n > 0) {
            return std::filesystem::path(std::string(path, n)).filename().string();
        }
#endif
    }
    const std::string base = std::filesystem::path(lib).filename().string();
    if (pe) {
        return base.find('.') == std::string::npos ? base + ".dll" : base;
    }
    if (base.find(".so") != std::string::npos) {
        return base;
    }
    return (base.rfind("lib", 0) == 0 ? base : "lib" + base) + ".so";
}

static bool resolve_import(const std::string& library, const std::string& name,
                           const std::vector<std::string>& unscoped_libs,
                           std::unordered_map<std::string, void*>& handles,
                           bool can_load, bool pe, std::string& file, std::string& why) {
    const std::string c_runtime = pe ? "msvcrt" : "c";
    if (!library.empty()) {
        file = library_file_name(library, handles, can_load, pe);
        if (can_load && lookup_symbol(handles.at(library), name) == nullptr) {
            why = "extern \"" + name + "\" is not in \"" + library + "\", the library its module links";
            return false;
        }
        return true;
    }
    if (!can_load) {
        file = pe ? "msvcrt.dll" : "libc.so.6";
        return true;
    }
    std::string searched;
    for (const auto& lib : unscoped_libs) {
        if (lookup_symbol(handles.at(lib), name) != nullptr) {
            file = library_file_name(lib, handles, can_load, pe);
            return true;
        }
        searched += ", " + library_file_name(lib, handles, can_load, pe);
    }
    if (!handles.contains(c_runtime)) {
        std::string ignored;
        handles[c_runtime] = open_library(pe ? "msvcrt.dll" : "libc.so.6", ignored);
    }
    if (void* crt = handles.at(c_runtime); crt != nullptr && lookup_symbol(crt, name) != nullptr) {
        file = pe ? "msvcrt.dll" : "libc.so.6";
        return true;
    }
    why = "extern \"" + name + "\" is in none of " + std::string(pe ? "msvcrt.dll" : "libc.so.6") + searched +
          "; put it in a module that links its library, or name the library with -l";
    return false;
}

void display_help() {
    std::cout << "Usage: occultc [options] <source.occ>\n"
              << "Info: Occult defaults to its JIT mode.\n"
              << "Options:\n"
              << "  -t,   --time              Show compilation time per stage\n"
              << "  -d,   --debug             Enable debug mode (implies --time)\n"
              << "  -o,   --output <file>     Output native binary: static when it calls no\n"
              << "                            library, dynamic (ELF) or importing (PE) when\n"
              << "                            it does. No linker or toolchain needed\n"
              << "  -c,   --compile-only      Output an object file -- ELF .o or COFF .obj,\n"
              << "                            per --format -- for cc or cl to link\n"
              << "  -W,   --writable-text     Mark .text writable (with -c), for a db\n"
              << "                            block the program writes into\n"
              << "  -l,   --load <lib>        A library for the `extern fn`s outside any\n"
              << "                            module that links one: opened for the JIT,\n"
              << "                            and needed by the image -o writes. Repeatable;\n"
              << "                            -lz, -l z and -l libz.so.1 all work. A module\n"
              << "                            can name its own: module z link \"z\" { ... }\n"
              << "        --abi <sysv|win64>  Calling convention (default: host)\n"
              << "        --format <elf|coff> Object file format for -c (default: this\n"
                 "                            host's). The format picks the ABI unless\n"
                 "                            --abi says otherwise. Link a coff object\n"
                 "                            with cl or clang, not link.exe: the driver\n"
                 "                            is what passes the C runtime\n"
              << "        --no-drectve        Leave the MSVC /DEFAULTLIB directives out\n"
                 "                            of a coff object. For a toolchain that\n"
                 "                            names its own libraries, such as\n"
                 "                            mingw-w64, whose ld warns about them\n"
              << "  -h,   --help              Show this message\n";
}

int main(int argc, char* argv[]) {
    std::string input_file;
    std::string source_original;

    bool debug = false;
    bool verbose = false;
    bool showtime = false;
    bool jit = true;
    bool object_out = false;
    bool writable_text = false;
#ifdef _WIN64
    bool coff_out = true;
#else
    bool coff_out = false;
#endif
    bool abi_was_given = false;
    bool defaultlibs = true;

    std::string filenameout;
    std::vector<std::string> prog_args;
    std::vector<std::string> load_libs;
    occult::x86_64::call_abi abi = occult::x86_64::host_abi();

    for (int i = 1; i < argc; ++i) {
        if (std::string arg = argv[i]; arg == "--") {
            for (int j = i + 1; j < argc; ++j) {
                prog_args.emplace_back(argv[j]);
            }
            break;
        }
        else if (arg == "-d" || arg == "--debug") {
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
                filenameout = argv[++i];
            }
            else {
                filenameout = "a.out";
            }
        }
        else if (arg == "-W" || arg == "--writable-text") {
            writable_text = true;
        }
        else if ((arg == "-l" || arg == "--load") && i + 1 < argc) {
            load_libs.emplace_back(argv[++i]);
        }
        else if (arg.rfind("-l", 0) == 0 && arg.size() > 2) {
            load_libs.emplace_back(arg.substr(2)); // -lz, joined the way cc takes it
        }
        else if (arg == "-c" || arg == "--compile-only") {
            jit = false;
            object_out = true;
        }
        else if (arg == "--abi" && i + 1 < argc) {
            const std::string v = argv[++i];
            abi_was_given = true;
            if (v == "win64") abi = occult::x86_64::call_abi::win64;
            else if (v == "sysv") abi = occult::x86_64::call_abi::sysv;
            else { std::cerr << "Unknown ABI: " << v << " (expected sysv or win64)\n"; return 1; }
        }
        else if (arg == "--format" && i + 1 < argc) {
            const std::string v = argv[++i];
            if (v == "coff") coff_out = true;
            else if (v == "elf") coff_out = false;
            else { std::cerr << "Unknown object format: " << v << " (expected elf or coff)\n"; return 1; }
        }
        else if (arg == "--no-drectve") {
            defaultlibs = false;
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
        std::cerr << RED << "[-] No input file specified" << RESET << std::endl;
        display_help();
        return 0;
    }

    if (!abi_was_given) {
        abi = coff_out ? occult::x86_64::call_abi::win64 : occult::x86_64::call_abi::sysv;
    }

    occult::target_win64 = jit ? occult::target_win64 : coff_out;

    // Program argv: source file stands in for argv[0].
    std::vector<char*> prog_argv;
    prog_argv.push_back(const_cast<char*>(input_file.c_str()));
    for (auto& a : prog_args) {
        prog_argv.push_back(const_cast<char*>(a.c_str()));
    }
    prog_argv.push_back(nullptr);

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
        std::cerr << RED << "[OCCULTC] Parsing failed with " << parser.get_error_count() << " error(s)" << RESET << std::endl;
        return 1;
    }
    if (showtime) {
        std::cout << GREEN << "[OCCULTC] Completed parsing \033[0m" << duration.count() << "ms\n";
    }
    try {
        occult::lower_error_builtin(cst.get(), occult::target_win64);
    }
    catch (const std::exception& e) {
        std::cerr << RED << "[OCCULTC] " << e.what() << RESET << std::endl;
        return 1;
    }

    std::vector<std::pair<std::string, std::string>> occult_exports;
    {
        std::function<void(occult::cst*)> scan = [&](occult::cst* n) {
            if (n->get_type() == occult::cst_type::function && !n->get_children().empty()) {
                bool is_exported = false;
                for (auto& k : n->get_children()) {
                    if (k->get_type() == occult::cst_type::func_is_exported) {
                        is_exported = true;
                        break;
                    }
                }
                if (is_exported) {
                    auto* name_node = n->get_children().front().get();
                    if (name_node->get_type() == occult::cst_type::identifier) {
                        const std::string& qualified = name_node->content;
                        const auto at = qualified.rfind("::");
                        const std::string leaf = (at == std::string::npos) ? qualified : qualified.substr(at + 2);
                        occult_exports.emplace_back(qualified, leaf);
                    }
                }
            }
            for (auto& k : n->get_children()) scan(k.get());
        };
        scan(cst.get());
    }
    const bool is_shared_library = !occult_exports.empty();
    if (debug && verbose) {
        cst->visualize();
    }

    occult::linter linter(cst.get(), debug);
    const bool lint_ok = linter.analyze();
    for (const auto& err : linter.get_errors()) {
        const char* prefix = (err.level == occult::lint_error::severity::error) ? RED "[LINT ERROR] " RESET : YELLOW "[LINT WARN]  " RESET;
        std::cerr << prefix << err.message << "\n";
    }
    if (!lint_ok) {
        std::cerr << RED << "[OCCULTC] Linting failed \u2014 " << linter.get_errors().size() << " error(s)" << RESET << "\n";
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

    // -c leaves libraries to whatever links the object
    if (object_out && !load_libs.empty()) {
        std::cerr << YELLOW << "[OCCULTC] -l is ignored with -c: an object file's "
                  << "libraries are the linker's to find" << RESET << std::endl;
        load_libs.clear();
    }

    std::vector<std::string> link_libraries;
    for (const auto& f : ir) {
        if (f.is_external && !f.library.empty() &&
            std::find(link_libraries.begin(), link_libraries.end(), f.library) == link_libraries.end()) {
            link_libraries.push_back(f.library);
        }
    }

#ifdef __linux__
    const bool host_is_target = jit || !coff_out;
#elif defined(_WIN64)
    const bool host_is_target = jit || coff_out;
#else
    const bool host_is_target = jit;
#endif
    std::unordered_map<std::string, void*> library_handles;
    if (host_is_target && !object_out) {
        for (const auto& lib : load_libs) {
            std::string why;
            void* h = open_library(lib, why);
            if (h == nullptr) {
                std::cerr << RED << "[OCCULTC] Could not load " << lib << ": " << RESET << why << std::endl;
                return 1;
            }
            library_handles[lib] = h;
        }
        for (const auto& lib : link_libraries) {
            std::string why;
            void* h = open_library(lib, why);
            if (h == nullptr) {
                std::cerr << RED << "[OCCULTC] Could not load \"" << lib << "\", which a module links: " << RESET
                          << why << std::endl;
                return 1;
            }
            library_handles[lib] = h;
        }
    }

    start = std::chrono::high_resolution_clock::now();
    std::optional<occult::x86_64::codegen_v2> codegen_holder;

    try {
        codegen_holder.emplace(ir, ir_structs, debug, abi, ir_gen.program_globals);
        codegen_holder->image_imports = !jit && !object_out;
        codegen_holder->library_handles = library_handles;
        codegen_holder->compile(jit, object_out);
    }
    catch (const std::exception& e) {
        std::cerr << RED << "[OCCULTC] Code generation failed: " << RESET << e.what() << std::endl;
        return 1;
    }

    auto& codegen_v2 = *codegen_holder;
    // what main returned, to exit with; the OS keeps the low eight bits
    int exit_code = 0;
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
                auto main_fn = codegen_v2.get_function<std::int64_t(*)(std::int64_t, std::int64_t)>("main");
                res = main_fn(static_cast<std::int64_t>(prog_argv.size() - 1),
                              reinterpret_cast<std::int64_t>(prog_argv.data()));
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

            exit_code = static_cast<int>(res);

            if (debug) {
                std::cout << "Main returned: " << res << std::endl;
            }

            if (showtime) {
                std::cout << GREEN << "[OCCULTC] Completed executing jit code " << RESET << duration.count() << "ms\n";
            }
        }
        else {
            std::cerr << RED << "[OCCULTC] No main function to run" << RESET << std::endl;

            return 1;
        }
    }
#else
    if (jit) {
        if (auto it = codegen_v2.function_locs.find("main"); it != codegen_v2.function_locs.end()) {
            start = std::chrono::high_resolution_clock::now();

            std::int64_t res = 0;
#ifdef _WIN64
            void* veh = AddVectoredExceptionHandler(1, jit_exception_filter);
            if (setjmp(jit_jmp_buf) != 0) {
                RemoveVectoredExceptionHandler(veh);
                std::cerr << RED << "[OCCULTC] JIT execution crashed (exception 0x" << std::hex
                          << jit_signal_caught << std::dec << ")" << RESET << std::endl;

                return 1;
            }
#endif
            if (codegen_v2.function_locs.count("__global_init")) {
                codegen_v2.get_function<std::int64_t (*)()>("__global_init")();
            }
            res = codegen_v2.get_function<std::int64_t (*)(std::int64_t, std::int64_t)>("main")(
                static_cast<std::int64_t>(prog_argv.size() - 1),
                reinterpret_cast<std::int64_t>(prog_argv.data()));
#ifdef _WIN64
            RemoveVectoredExceptionHandler(veh);
#endif

            end = std::chrono::high_resolution_clock::now();
            duration = end - start;

            exit_code = static_cast<int>(res);

            if (debug) {
                std::cout << "Main returned: " << res << std::endl;
            }

            if (showtime) {
                std::cout << GREEN << "[OCCULTC] Completed executing jit code " << RESET << duration.count() << "ms\n";
            }
        }
        else {
            std::cerr << RED << "[OCCULTC] No main function to run" << RESET << std::endl;

            return 1;
        }
    }
#endif
    if (!jit && object_out) {
        if (filenameout.empty()) {
            filenameout = coff_out ? "a.obj" : "a.o";
        }
        std::vector<occult::elf_object::reloc> relocs;
        relocs.reserve(codegen_v2.object_relocs.size());
        for (const auto& r : codegen_v2.object_relocs) {
            relocs.push_back({r.symbol,
                              static_cast<occult::elf_object::against>(r.section),
                              r.at,
                              static_cast<occult::elf_object::kind>(r.kind),
                              r.addend});
        }
        if (coff_out) {
            occult::coff_object::write(filenameout, codegen_v2.get_code(), codegen_v2.function_locs,
                                       codegen_v2.undefined_symbols, relocs, codegen_v2.string_literals,
                                       codegen_v2.string_relocs, codegen_v2.global_relocs,
                                       codegen_v2.globals(), codegen_v2.globals_size(), "__global_init",
                                       writable_text, debug, defaultlibs, codegen_v2.used_libraries);
        }
        else {
            occult::elf_object::write(filenameout, codegen_v2.get_code(), codegen_v2.function_locs,
                                      codegen_v2.undefined_symbols, relocs, codegen_v2.string_literals,
                                      codegen_v2.string_relocs, codegen_v2.global_relocs,
                                      codegen_v2.globals(), codegen_v2.globals_size(), "__global_init",
                                      writable_text, debug);
        }
        if ((!coff_out || !defaultlibs) && !codegen_v2.used_libraries.empty()) {
            std::string flags;
            for (const auto& lib : codegen_v2.used_libraries) {
                flags += " " + cc_library_flag(lib);
            }
            std::cerr << YELLOW << "[OCCULTC] Link " << filenameout << " with" << flags
                      << ", the libraries its modules name" << RESET << std::endl;
        }
    }
    else if (!jit) {
        std::vector<std::string> unscoped_libs = load_libs;
        if (coff_out) {
            std::vector<occult::pe_import_use> imports;
            for (const auto& use : codegen_v2.import_thunks) {
                std::string dll;
                std::string why;
                if (!resolve_import(use.library, use.name, unscoped_libs, library_handles, host_is_target, true, dll, why)) {
                    std::cerr << RED << "[OCCULTC] " << why << RESET << std::endl;
                    return 1;
                }
                imports.push_back({dll, use.name, use.field});
            }
            if (is_shared_library) {
                if (codegen_v2.function_locs.contains("main")) {
                    std::cerr << RED << "[OCCULTC] a program can be either an executable "
                                        "(with main) or a shared library (with export fn), not both"
                              << RESET << std::endl;
                    return 1;
                }
                occult::linker::link_blob_dll(filenameout, codegen_v2.get_code(), codegen_v2.function_locs,
                                              codegen_v2.string_relocs, codegen_v2.string_literals,
                                              codegen_v2.global_relocs, codegen_v2.globals_size(),
                                              occult_exports, imports, debug, showtime);
            }
            else {
                occult::linker::link_blob_pe(filenameout, codegen_v2.get_code(), codegen_v2.function_locs, codegen_v2.string_relocs, codegen_v2.string_literals, codegen_v2.global_relocs, codegen_v2.globals_size(),
                                             codegen_v2.function_arity("main") > 0, imports, debug, showtime);
            }
        }
        else {
            std::vector<occult::elf_import_use> imports;
            std::vector<std::string> needed;
            auto need = [&](const std::string& soname) {
                if (std::find(needed.begin(), needed.end(), soname) == needed.end()) {
                    needed.push_back(soname);
                }
            };
            for (const auto& use : codegen_v2.import_thunks) {
                std::string soname;
                std::string why;
                if (!resolve_import(use.library, use.name, unscoped_libs, library_handles, host_is_target, false, soname, why)) {
                    std::cerr << RED << "[OCCULTC] " << why << RESET << std::endl;
                    return 1;
                }
                need(soname);
                imports.push_back({use.name, use.field});
            }
            if (!imports.empty()) {
                for (const auto& lib : load_libs) {
                    need(library_file_name(lib, library_handles, host_is_target, false));
                }
                need("libc.so.6");
            }

            if (is_shared_library) {
                if (codegen_v2.function_locs.contains("main")) {
                    std::cerr << RED << "[OCCULTC] a program can be either an executable "
                                        "(with main) or a shared library (with export fn), not both"
                              << RESET << std::endl;
                    return 1;
                }
                occult::linker::link_blob_so(filenameout, codegen_v2.get_code(), codegen_v2.function_locs,
                                             codegen_v2.string_relocs, codegen_v2.string_literals,
                                             codegen_v2.global_relocs, codegen_v2.globals_size(),
                                             occult_exports, imports, needed, debug, showtime);
            }
            else {
                occult::linker::link_blob(filenameout, codegen_v2.get_code(), codegen_v2.function_locs, codegen_v2.string_relocs, codegen_v2.string_literals, codegen_v2.global_relocs, codegen_v2.globals_size(), debug, showtime,
                                          imports, needed);

#ifdef __linux
                chmod(filenameout.c_str(), S_IRWXU);
#endif
            }
        }
    }

    return exit_code;
}
