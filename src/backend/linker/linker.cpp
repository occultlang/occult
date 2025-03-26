#include "linker.hpp"

namespace occult {
  void linker::link_and_create_binary(const std::string& binary_name, std::unordered_map<std::string, jit_function>& function_map,
                                      const std::map<std::string, std::vector<std::uint8_t>>& function_raw_code_map) {
    std::unordered_map<std::uint64_t, std::string> func_addr_map; 
    
    std::cout << GREEN << "[*] Linking functions...\n" << RESET;
    
    for (const auto& pair : function_map) { // Copy address of functions to map the address itself to the name of the function
      func_addr_map[reinterpret_cast<std::uint64_t>(&pair.second)] = pair.first;
    }
    
    std::vector<std::uint8_t> final_code;
    std::unordered_map<std::string, std::uint64_t> locations;
    
    std::size_t offset = 0;
    for (const auto& m : function_raw_code_map) {
      locations[m.first] = offset; // Store the correct function start offset
      final_code.insert(final_code.end(), m.second.begin(), m.second.end());
      std::cout << BLUE << "[+] Added function: " << m.first << RESET << std::endl;
      offset += m.second.size(); // Move offset forward
    }
    
    const std::uint64_t base_addr = 0x400000;
    std::uint64_t entry_addr = base_addr + locations["main"] + sizeof(elf_header) + sizeof(elf_program_header);
    
    std::cout << GREEN << "[*] Base address: " << YELLOW << "0x" << std::hex << base_addr << std::dec << RESET << std::endl;
    std::cout << GREEN << "[*] Entry address: " << YELLOW << "0x" << std::hex << entry_addr << std::dec << RESET << std::endl;
    
    // Scan for call relocations
    for (std::size_t i = 0; i < final_code.size() - 11; i++) {
      auto current_byte = final_code.at(i);
      auto next_byte = final_code.at(i + 1);
      auto second_last_byte = final_code.at(i + 10);
      auto last_byte = final_code.at(i + 11);
      
      // Check for the byte sequence for 'call' (48 b8 XX XX XX XX XX XX XX XX ff 10 or ff 13)
      if (current_byte == 0x48 && (next_byte == 0xb8 || next_byte == 0xbb) && second_last_byte == 0xff && (last_byte == 0x10 || last_byte == 0x13)) {
          std::cout << CYAN << "[!] Byte sequence found at index " << i << RESET << "\n";
          
          std::cout << CYAN << "    Old byte sequence: ";
          for (std::size_t j = 0; j <= 11; j++) {
            std::cout << std::setw(2) << std::setfill('0') << std::uppercase << std::hex << static_cast<int>(final_code.at(i + j)) << " ";
          }
          std::cout << std::dec << RESET << "\n";
          
          // Extract the 8-byte address
          std::uint64_t address = 0;
          for (std::size_t j = 2; j <= 9; j++) {
            address |= static_cast<std::uint64_t>(final_code.at(i + j)) << ((j - 2) * 8);
          }
          
          std::cout << YELLOW << "    Address to reloc: 0x" << std::hex << address << std::dec << RESET << "\n";
          std::cout << BLUE << "    Resolving: " << func_addr_map[address] << RESET << "\n";
          
          // Calculate the new resolved address based on the symbol location
          std::uint64_t new_address = base_addr + locations[func_addr_map[address]] + sizeof(elf_header) + sizeof(elf_program_header);
  
          std::cout << YELLOW << "    New address: 0x" << std::hex << new_address << std::dec << RESET << "\n";
          
          // Replace the address bytes with the new relative offset
          for (std::size_t j = 0; j < sizeof(new_address); ++j) {
            final_code.at(i + 2 + j) = static_cast<std::uint8_t>((new_address >> (j * 8)) & 0xFF);
          }
          
          std::cout << CYAN << "    New byte sequence: ";
          for (std::size_t j = 0; j <= 11; j++) {
            std::cout << std::setw(2) << std::setfill('0') << std::uppercase << std::hex << static_cast<int>(final_code.at(i + j)) << " ";
          }
          std::cout << std::dec << RESET << "\n";
        }
    }
  
    // Generate the final binary
    elf::generate_binary(binary_name, final_code, final_code.size() * 2, entry_addr);
  }
}
