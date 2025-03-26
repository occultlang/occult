#include "linker.hpp"

namespace occult {
  void linker::link_and_create_binary(const std::string& binary_name, std::unordered_map<std::string, jit_function>& function_map,
                                      const std::map<std::string, std::vector<std::uint8_t>>& function_raw_code_map) {
    std::unordered_map<std::uint64_t, std::string> func_addr_map; 
    
    for (const auto& pair : function_map) { // copy address of functions to map the address itself to the name of the function
      func_addr_map[reinterpret_cast<std::uint64_t>(&pair.second)] = pair.first;
    }
    
    std::vector<std::uint8_t> final_code;
    std::unordered_map<std::string, std::uint64_t> locations;
    
    for (const auto& m : function_raw_code_map) {
      final_code.insert(final_code.end(), m.second.begin(), m.second.end());
      std::cout << m.first << std::endl;
      locations[m.first] = final_code.size();
    }
    
    
    
    const std::uint64_t base_addr = 0x400078;
    std::uint64_t entry_addr = base_addr - locations["main"];
    
    /* HOW TO: Call Reloc
     * Scan through and check byte sequence 48 b8 XX XX XX XX XX XX XX XX ff 10
     * And then fix the address to the correct location in final_code (marked by XX)
     * This is extremely hacky I think...
    */
    for (std::size_t i = 0; i < final_code.size() - 11; i++) {
      auto current_byte = final_code.at(i);
      auto next_byte = final_code.at(i + 1);
      auto second_last_byte = final_code.at(i + 10);
      auto last_byte = final_code.at(i + 11);
      
      if (current_byte == 0x48 && (next_byte == 0xb8 || next_byte == 0xbb) && second_last_byte == 0xff && (last_byte == 0x10 || last_byte == 0x13)) {
        std::cout << "call reloc byte sequence found!\n";
        
        std::cout << "old byte sequence: ";
        for (std::size_t j = 0; j <= 11; j++) {
          std::cout << std::setw(2) << std::setfill('0') << std::uppercase << std::hex << static_cast<int>(final_code.at(i + j)) << " ";
        }
        std::cout << std::dec << "\n";
        
        // location range for bytes (2 to 9)
        std::uint64_t address = 0;
        for (std::size_t j = 2; j <= 9; j++) {
          address |= static_cast<std::uint64_t>(final_code.at(i + j)) << ((j - 2) * 8);
        }
        
        std::cout << "address to reloc: 0x" << std::hex << address << std::dec << "\n";
        std::cout << "0x" << std::hex << address << std::dec << " points to " << func_addr_map[address] << "\n";
        
        std::uint64_t new_address = locations[func_addr_map[address]] + base_addr; // locations[func_name]
        
        std::cout << "new address: 0x" << std::hex << new_address << std::dec << "\n";
        
        std::vector<std::uint8_t> new_bytes(sizeof(new_address)); // extract bytes from new addr
        for (std::size_t i = 0; i < sizeof(new_address); ++i) {
          new_bytes[i] = static_cast<std::uint8_t>((new_address >> (i * 8)) & 0xFF);
        }
        
        for (std::size_t j = 0; j < new_bytes.size(); ++j) { // copy bytes to final_code
          final_code.at(i + 2 + j) = new_bytes[j];
        }
        
        std::cout << "new byte sequence: ";
        for (std::size_t j = 0; j <= 11; j++) {
          std::cout << std::setw(2) << std::setfill('0') << std::uppercase << std::hex << static_cast<int>(final_code.at(i + j)) << " ";
        }
        std::cout << std::dec << "\n";
      }
    }
  
    elf::generate_binary(binary_name, final_code, final_code.size(), entry_addr);
  }
}
