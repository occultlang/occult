#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>
#include "../codegen/x86_64_writer.hpp"
#include "elf_header.hpp"

namespace occult {

    class linker {
    public:
        static void link_blob(const std::string& binary_name, const std::vector<std::uint8_t>& code, const std::unordered_map<std::string, std::uint32_t>& function_locs,
                              const std::vector<std::pair<std::size_t, std::uint64_t>>& string_relocs, const std::unordered_map<std::uint64_t, std::string>& string_literals,
                              const std::vector<std::pair<std::size_t, std::int32_t>>& global_relocs = {}, std::int32_t globals_size = 0,
                              bool debug = false, bool showtime = false);
    };

} // namespace occult
