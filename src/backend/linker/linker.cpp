#include "linker.hpp"
#include <chrono>

namespace occult {
void linker::link_and_create_binary(
    const std::string &binary_name,
    std::unordered_map<std::string, jit_function> &function_map,
    const std::map<std::string, std::vector<std::uint8_t>>
        &function_raw_code_map,
    const std::unordered_map<std::uint64_t, std::string> &string_literals,
    bool debug, bool showtime) {
  auto start = std::chrono::high_resolution_clock::now();

  std::unordered_map<std::uint64_t, std::string> func_addr_map;
  for (const auto &[name, fn] : function_map)
    func_addr_map[reinterpret_cast<std::uint64_t>(fn)] = name;

  std::vector<std::uint8_t> final_code;
  std::unordered_map<std::string, std::uint64_t> locations;

  constexpr std::size_t start_stub_size = 24;
  final_code.resize(start_stub_size, 0x90);

  for (const auto &[name, code] : function_raw_code_map) {
    locations[name] = final_code.size();
    final_code.insert(final_code.end(), code.begin(), code.end());

    if (debug)
      std::cout << BLUE << "[LINKER INFO] Added function: " << YELLOW << "\""
                << name << "\"" << RESET << "\n";
  }

  const std::size_t text_size = final_code.size();

  if (locations.find("main") == locations.end()) {
    std::cerr << RED << "[LINKER ERROR] No 'main' function found\n" << RESET;
    return;
  }

  constexpr std::uint64_t base_addr = 0x400000;
  constexpr std::uint64_t header_size =
      sizeof(elf_header) + sizeof(elf_program_header);
  const std::uint64_t entry_addr = base_addr + header_size;

  if (debug) {
    std::cout << BLUE << "[LINKER INFO] Base address: " << RED << "0x"
              << std::hex << base_addr << std::dec << RESET << "\n";
    std::cout << BLUE << "[LINKER INFO] Entry address: " << RED << "0x"
              << std::hex << entry_addr << std::dec << RESET << "\n";
  }

  const std::uint64_t main_addr = base_addr + header_size + locations["main"];

  // _start trampoline
  final_code[0] = 0x48;
  final_code[1] = 0xB8;
  for (std::uint8_t j = 0; j < 8; ++j) {
    final_code[2 + j] =
        static_cast<std::uint8_t>((main_addr >> (j * 8)) & 0xFF);
  }
  final_code[10] = 0xFF;
  final_code[11] = 0xD0;
  final_code[12] = 0x48;
  final_code[13] = 0x89;
  final_code[14] = 0xC7;
  final_code[15] = 0x48;
  final_code[16] = 0xC7;
  final_code[17] = 0xC0;
  final_code[18] = 0x3C;
  final_code[19] = 0x00;
  final_code[20] = 0x00;
  final_code[21] = 0x00;
  final_code[22] = 0x0F;
  final_code[23] = 0x05;

  std::unordered_map<std::uint64_t, std::uint64_t> relocation_map;

  for (const auto &[old_addr, content] : string_literals) {
    const std::uint64_t str_offset = final_code.size();
    const std::uint64_t len = content.size();

    for (std::uint8_t j = 0; j < 8; ++j)
      final_code.push_back(static_cast<std::uint8_t>((len >> (j * 8)) & 0xFF));

    final_code.insert(final_code.end(), content.begin(), content.end());
    final_code.push_back(0);

    const std::uint64_t new_data_addr =
        base_addr + header_size + str_offset + 8;
    relocation_map[old_addr] = new_data_addr;

    if (debug)
      std::cout << BLUE << "[LINKER INFO] Placed string \"" << content
                << "\" -> 0x" << std::hex << new_data_addr << std::dec << RESET
                << "\n";
  }

  for (std::size_t i = 0; i + 11 < final_code.size(); ++i) {
    const std::uint8_t b1 = final_code[i];
    const std::uint8_t b2 = final_code[i + 1];

    const bool is_movabs_rax = (b1 == 0x48 && b2 == 0xB8);
    const bool call_2b = is_movabs_rax && i + 11 < final_code.size() &&
                         final_code[i + 10] == 0xFF &&
                         final_code[i + 11] == 0xD0;
    const bool call_3b = is_movabs_rax && i + 12 < final_code.size() &&
                         final_code[i + 10] == 0x48 &&
                         final_code[i + 11] == 0xFF &&
                         final_code[i + 12] == 0xD0;

    if (call_2b || call_3b) {
      std::uint64_t old_addr = 0;
      for (std::uint8_t j = 0; j < 8; ++j)
        old_addr |= static_cast<std::uint64_t>(final_code[i + 2 + j])
                    << (j * 8);

      if (auto it = func_addr_map.find(old_addr); it != func_addr_map.end()) {
        const std::string &name = it->second;
        const std::uint64_t new_addr =
            base_addr + header_size + locations[name];

        for (std::uint8_t j = 0; j < 8; ++j)
          final_code[i + 2 + j] = (new_addr >> (j * 8)) & 0xFF;

        if (debug)
          std::cout << BLUE << "[LINKER INFO] Patched call to \"" << name
                    << "\" -> 0x" << std::hex << new_addr << std::dec << RESET
                    << "\n";
      }
      i += call_3b ? 12 : 10;
      continue;
    }

    if ((b1 == 0x48 || b1 == 0x49) && b2 >= 0xB8 && b2 <= 0xBF) {
      std::uint64_t old_addr = 0;
      for (std::uint8_t j = 0; j < 8; ++j)
        old_addr |= static_cast<std::uint64_t>(final_code[i + 2 + j])
                    << (j * 8);

      if (auto it = relocation_map.find(old_addr); it != relocation_map.end()) {
        const std::uint64_t new_addr = it->second;
        for (std::uint8_t j = 0; j < 8; ++j)
          final_code[i + 2 + j] = (new_addr >> (j * 8)) & 0xFF;

        if (debug)
          std::cout << BLUE << "[LINKER INFO] Patched string pointer -> 0x"
                    << std::hex << new_addr << std::dec << RESET << "\n";
      }
    }
  }

  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double, std::milli> duration = end - start;

  if (showtime)
    std::cout << GREEN << "[OCCULTC] Completed linking functions \033[0m"
              << duration.count() << "ms\n";

  elf::generate_binary(binary_name, final_code, entry_addr, base_addr,
                       base_addr, 0, text_size);
}
} // namespace occult
