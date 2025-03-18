#include "lexer/lexer.hpp"
#include "parser/ast.hpp"
#include "parser/parser.hpp"
#include "backend/ir_gen.hpp"
#include "backend/x64writer.hpp"
#include "backend/jit.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#ifdef __linux
#include <sys/stat.h>
#include "backend/elf_header.hpp"
#elif _WIN64
#include "backend/pe_header.hpp"
#include <winnt.h>
#include <winternl.h>
#endif

/*
    TODO: 
    
    add a map for posix syscalls
    move the nt dumping syscalls into a seperate file

    add sib byte to x64writer
    and other things if remembered
*/

std::unordered_map<std::string, std::int64_t> DumpAllSyscalls() {
    HMODULE hNtdll = GetModuleHandleA("ntdll.dll");

    if (!hNtdll) {
        std::cerr << "Failed to get ntdll.dll handle." << std::endl;
        return {};
    }

    PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)hNtdll;
    PIMAGE_NT_HEADERS ntHeaders = (PIMAGE_NT_HEADERS)((BYTE*)hNtdll + dosHeader->e_lfanew);
    PIMAGE_EXPORT_DIRECTORY exportDir = (PIMAGE_EXPORT_DIRECTORY)((BYTE*)hNtdll + ntHeaders->OptionalHeader.DataDirectory[0].VirtualAddress);

    DWORD* functionNames = (DWORD*)((BYTE*)hNtdll + exportDir->AddressOfNames);
    DWORD* functionAddresses = (DWORD*)((BYTE*)hNtdll + exportDir->AddressOfFunctions);
    PWORD nameOrdinals = (PWORD)((BYTE*)hNtdll + exportDir->AddressOfNameOrdinals);

    std::unordered_map<std::string, std::int64_t> syscall_map;

    for (size_t i = 0; i < exportDir->NumberOfNames; ++i) {
        std::string currentFunctionName = (char*)((BYTE*)hNtdll + functionNames[i]);

        if ((currentFunctionName.substr(0, 2).find("Nt") != std::string::npos)) {
            WORD ordinal = nameOrdinals[i];
            void* func_address = (void*)((BYTE*)hNtdll + functionAddresses[ordinal]);

            auto syscallNumber = (reinterpret_cast<std::uint8_t*>(func_address)[7] << 24) |
                (reinterpret_cast<std::uint8_t*>(func_address)[6] << 16) |
                (reinterpret_cast<std::uint8_t*>(func_address)[5] << 8) |
                reinterpret_cast<std::uint8_t*>(func_address)[4];

            syscall_map[currentFunctionName] = syscallNumber;
        }
    }

    return syscall_map;
}

std::unordered_map<std::string, std::uint64_t> DumpAllNtFunctions() {
    HMODULE hNtdll = GetModuleHandleA("ntdll.dll");

    if (!hNtdll) {
        std::cerr << "Failed to get ntdll.dll handle." << std::endl;
        return {};
    }

    PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)hNtdll;
    PIMAGE_NT_HEADERS ntHeaders = (PIMAGE_NT_HEADERS)((BYTE*)hNtdll + dosHeader->e_lfanew);
    PIMAGE_EXPORT_DIRECTORY exportDir = (PIMAGE_EXPORT_DIRECTORY)((BYTE*)hNtdll + ntHeaders->OptionalHeader.DataDirectory[0].VirtualAddress);

    DWORD* functionNames = (DWORD*)((BYTE*)hNtdll + exportDir->AddressOfNames);
    DWORD* functionAddresses = (DWORD*)((BYTE*)hNtdll + exportDir->AddressOfFunctions);
    PWORD nameOrdinals = (PWORD)((BYTE*)hNtdll + exportDir->AddressOfNameOrdinals);

    std::unordered_map<std::string, std::uint64_t> func_map;

    for (size_t i = 0; i < exportDir->NumberOfNames; ++i) {
        std::string currentFunctionName = (char*)((BYTE*)hNtdll + functionNames[i]);

        if ((currentFunctionName.substr(0, 2).find("Nt") != std::string::npos)) {
            WORD ordinal = nameOrdinals[i];
            void* func_address = (void*)((BYTE*)hNtdll + functionAddresses[ordinal]);

            func_map[currentFunctionName] = reinterpret_cast<std::uint64_t>(func_address);
        }
    }

    return func_map;
}

void display_help() {
  std::cout << "Usage: occultc [options] <source.occ>\n";
  std::cout << "Options:\n";
  std::cout << "  -t, --time                     Shows the compilation time for each stage.\n";
  std::cout << "  -d, --debug                    Enable debugging options (shows time as well -t is not needed)\n";
  std::cout << "  -h, --help                     Display this help message." << std::endl;
}

int main(int argc, char* argv[]) {
  std::string input_file;
  std::string source_original;
  
  bool debug = false;
  bool verbose = false;
  bool showtime = false;
  
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    
    if (arg == "-d" || arg == "--debug") {
      debug = true;
      verbose = true;
      showtime = true;
    }
    else if (arg == "-t" || arg == "--time") {
      showtime = true;
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
    std::cout << "No input file specified" << std::endl;
    display_help();
    
    //return 0;
  }
  
  auto start = std::chrono::high_resolution_clock::now();
  occult::lexer lexer(source_original);
  auto tokens = lexer.analyze();
  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double, std::milli> duration = end - start;
  if (showtime) {
    std::cout << "[occultc] \033[1;35mcompleted lexical analysis \033[0m" << duration.count() << "ms\n";
  }
  if (debug && verbose) {
    lexer.visualize();
  }
  
  start = std::chrono::high_resolution_clock::now();
  occult::parser parser(tokens);
  auto ast = parser.parse();
  end = std::chrono::high_resolution_clock::now();
  duration = end - start;
  if (showtime) {
     std::cout << "[occultc] \033[1;35mcompleted parsing \033[0m" << duration.count() << "ms\n";
  }
  if (debug && verbose) {
    ast->visualize();
  }

  start = std::chrono::high_resolution_clock::now();
  occult::ir_gen ir_gen(ast.get());
  auto ir = ir_gen.lower();
  end = std::chrono::high_resolution_clock::now();
  duration = end - start;
  if (showtime) {
     std::cout << "[occultc] \033[1;35mcompleted generating ir \033[0m" << duration.count() << "ms\n";
  }
  if (debug) {
    ir_gen.visualize(ir);
  }

  start = std::chrono::high_resolution_clock::now();
  occult::jit_runtime jit_runtime(ir, debug);
  jit_runtime.convert_ir();
  end = std::chrono::high_resolution_clock::now();
  duration = end - start;
  if (showtime) {
    std::cout << "[occultc] \033[1;35mcompleted converting ir to machine code \033[0m" << duration.count() << "ms\n";
  }
  if (debug) {
    for (const auto& pair : jit_runtime.function_map) {
      std::cout << pair.first << std::endl;
      std::cout << "0x" << std::hex << reinterpret_cast<std::int64_t>(&pair.second) << std::dec << std::endl;
    }
  }
  
  auto it = jit_runtime.function_map.find("main");
  if (it != jit_runtime.function_map.end()) {
    start = std::chrono::high_resolution_clock::now();
    
    auto res = it->second();
    
    end = std::chrono::high_resolution_clock::now();
    duration = end - start;
    
    if (debug) {
      std::cout << "JIT main returned: " << res << std::endl;
    }
    
    if (showtime) {
      std::cout << "[occultc] \033[1;35mcompleted executing jit code \033[0m" << duration.count() << "ms\n";
    }
  }
  else {
    std::cerr << "main function not found!" << std::endl;
  }

  system("cls");

  auto nt_syscall_map = DumpAllSyscalls();

  const char* str = "Hello, World!";
  IO_STATUS_BLOCK IoStatusBlock;

  occult::x64writer print;
  print.emit_function_prologue(0);
  print.emit_mov_reg_imm("r10", reinterpret_cast<std::int64_t>(GetStdHandle(STD_OUTPUT_HANDLE)), occult::k64bit_extended);
  print.emit_mov_reg_imm("rdx", NULL);
  print.emit_mov_reg_imm("r8", NULL, occult::k64bit_extended);
  print.emit_mov_reg_imm("r9", NULL, occult::k64bit_extended);

  print.emit_mov_reg_imm("rax", reinterpret_cast<std::int64_t>(&IoStatusBlock));
  print.push_bytes({ 0x48, 0x89, 0x44, 0x24, 40 }); // mov    QWORD PTR [rsp+40],rax 
  
  print.emit_mov_reg_imm("rax", reinterpret_cast<std::int64_t>(str));
  print.push_bytes({ 0x48, 0x89, 0x44, 0x24, 48 }); // mov    QWORD PTR [rsp+48],rax 

  print.emit_mov_reg_imm("rax", 14);
  print.push_bytes({ 0x48, 0x89, 0x44, 0x24, 56 }); // mov    QWORD PTR [rsp+56],rax 

  print.emit_mov_reg_imm("rax", 0);
  print.push_bytes({ 0x48, 0x89, 0x44, 0x24, 64 }); // mov    QWORD PTR [rsp+64],rax 
  print.push_bytes({ 0x48, 0x89, 0x44, 0x24, 72 }); // mov    QWORD PTR [rsp+72],rax 

  print.emit_mov_reg_imm("rax", nt_syscall_map["NtWriteFile"]);
  print.emit_syscall();
  print.emit_function_epilogue();
  print.emit_ret();

  print.setup_function()();

  return 0;
}
