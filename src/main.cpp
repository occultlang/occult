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
#endif

void display_help() {
  std::cout << "Usage: occultc [options] <source.occ>\n";
  std::cout << "Options:\n";
  std::cout << "  -t, --time                     Shows the compilation time for each stage.\n";
  std::cout << "  -d, --debug                    Enable debugging options (shows time as well -t is not needed)\n";
  std::cout << "  -h, --help                     Display this help message.\n";
}

int main(int argc, char* argv[]) {
  std::vector<double> times;
  
  std::string input_file;
  std::string source_original;
  
  bool debug = false;
  verbose = false;
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
      std::cout << "No input file specified\n";
      display_help();
      
      return 0;
  }
  
  auto start = std::chrono::high_resolution_clock::now();
  
  occult::lexer lexer(source_original);
  
  std::vector<occult::token_t> stream = lexer.analyze();
  
  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double, std::milli> duration = end - start;
  
  times.push_back(duration.count());
  
  if (showtime)
    std::cout << "[occultc] \033[1;35mcompleted lexical analysis \033[0m" << duration.count() << "ms\n";
  
  if (debug && verbose) {
    lexer.visualize();
  }

  occult::parser parser(stream);
  
  start = std::chrono::high_resolution_clock::now();
  
  auto root = parser.parse();
  
  end = std::chrono::high_resolution_clock::now();
  duration = end - start;
  
  times.push_back(duration.count());
  
  if (showtime)
    std::cout << "[occultc] \033[1;35mcompleted parsing \033[0m" << duration.count() << "ms\n";
  
  if (debug && verbose) {
    root->visualize();
  }
  
  start = std::chrono::high_resolution_clock::now();
  
  occult::ir_gen ir_gen(root.get());
  auto ir_funcs = ir_gen.generate();
  
  end = std::chrono::high_resolution_clock::now();
  duration = end - start;
  
  times.push_back(duration.count());
  
  if (showtime)
    std::cout << "[occultc] \033[1;35mcompleted generating ir \033[0m" << duration.count() << "ms\n";

  if (debug && verbose) {
    for (auto& func : ir_funcs) {
      std::cout << "\n" << func.type << "\n";
      std::cout << func.name << "\n";
      
      std::cout << "args:\n";
      for (auto& arg : func.args) {
        std::cout << "\t" << arg.type << "\n";
        std::cout << "\t" << arg.name << "\n";
      }
      
      struct visitor {
        void operator()(const float& v){ std::cout << v << "\n"; };
        void operator()(const double& v){ std::cout << v << "\n"; };
        void operator()(const std::int64_t& v){ std::cout << v << "\n"; };
        void operator()(const std::uint64_t& v){ std::cout << v << "\n"; };
        void operator()(const std::string& v){ std::cout << v << "\n"; };
        void operator()(std::monostate){ std::cout << "\n"; };
      };
      
      std::cout << "code:\n";
      for (auto& i : func.code) {
        std::cout << occult::opcode_to_string(i.op) << " ";
        std::visit(visitor(), i.operand);
      }
    }
  }
  
  start = std::chrono::high_resolution_clock::now();
  
  occult::jit jit(ir_funcs, debug);
  jit.convert_ir();
  
  end = std::chrono::high_resolution_clock::now();
  duration = end - start;
  
  times.push_back(duration.count());
  
  if (showtime)
    std::cout << "[occultc] \033[1;35mcompleted converting ir to machine code \033[0m" << duration.count() << "ms\n";
  
  if (debug) {
    for (const auto& pair : jit.function_map) {
      std::cout << pair.first << std::endl;
      std::cout << "0x" << std::hex << reinterpret_cast<std::int64_t>(&pair.second) << std::dec << std::endl;
    }
  }
  
  if (jit.function_map.find("main") != jit.function_map.end()) {
    start = std::chrono::high_resolution_clock::now();
    
    int ret = jit.function_map["main"]();
    
    end = std::chrono::high_resolution_clock::now();
    duration = end - start;
    
    times.push_back(duration.count());
    
    if (showtime) 
    
    if (debug) {
      std::cout << "main return value: " << ret << std::endl;
    }
  }
/*#ifdef __linux
  occult::elf::generate_binary("a.out", writer.get_code());
  
  if (chmod("a.out", S_IRUSR | S_IWUSR | S_IXUSR) != 0) {
    std::cerr << "failed to change permissions to binary" << std::endl;
    return 1;
  }
#endif*/
  
  return 0;
}
