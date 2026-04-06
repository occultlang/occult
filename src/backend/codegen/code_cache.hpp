#pragma once
#include <filesystem>
#include <fstream>
#include <map>
#include "../../third-party/xxhash/xxhash.h"
#include "bytecode_format.hpp"
#include "ir_gen.hpp"
#include "writer.hpp"

namespace occult {
    using IRHash = XXH128_hash_t;

    IRHash hash_ir_function(const ir_function& func) {
        XXH3_state_t* state = XXH3_createState();
        if (!state) {
            throw std::runtime_error("Abort hashing, invalid state");
        }

        XXH3_128bits_reset(state);

        const std::string version = "occult-ir-v1";
        XXH3_128bits_update(state, version.data(), version.size());
        XXH3_128bits_update(state, func.name.data(), func.name.size());
        XXH3_128bits_update(state, func.type.data(), func.type.size());
        XXH3_128bits_update(state, &func.uses_shellcode, sizeof(func.uses_shellcode));
        XXH3_128bits_update(state, &func.is_external, sizeof(func.is_external));
        XXH3_128bits_update(state, &func.is_variadic, sizeof(func.is_variadic));
        XXH3_128bits_update(state, &func.uses_assembly, sizeof(func.uses_assembly));

        for (const auto& arg : func.args) {
            XXH3_128bits_update(state, arg.name.data(), arg.name.size());
            XXH3_128bits_update(state, arg.type.data(), arg.type.size());
        }

        for (const auto& instr : func.code) {
            uint8_t op_val = static_cast<uint8_t>(instr.op);
            XXH3_128bits_update(state, &op_val, sizeof(op_val));

            std::visit(
                [&](auto&& value) {
                    using T = std::decay_t<decltype(value)>;

                    if constexpr (std::is_arithmetic_v<T>) {
                        XXH3_128bits_update(state, &value, sizeof(value));
                    }
                    else if constexpr (std::is_same_v<T, std::string>) {
                        XXH3_128bits_update(state, value.data(), value.size());
                    }
                },
                instr.operand);

            XXH3_128bits_update(state, instr.type.data(), instr.type.size());
        }

        IRHash final_hash = XXH3_128bits_digest(state);
        XXH3_freeState(state);

        return final_hash;
    }

    static constexpr std::string_view kCacheRootDir = "./occult_jit_cache/";

    inline std::string& cache_namespace_id() {
        static std::string ns = "default";
        return ns;
    }

    inline void set_cache_namespace(const std::string& key) {
        if (key.empty()) {
            cache_namespace_id() = "default";
            return;
        }
        const std::uint64_t key_hash = XXH3_64bits(key.data(), key.size());
        cache_namespace_id() = std::to_string(key_hash);
    }

    inline std::string cache_dir_path() { return std::string(kCacheRootDir) + cache_namespace_id() + "/"; }

    inline std::string cache_path_for(const std::string& func_name) { return cache_dir_path() + func_name + ".bin"; }

    inline void ensure_cache_dir() { std::filesystem::create_directories(cache_dir_path()); }

    bytecode_header generate_bytecode_header(const ir_function& func, arch target_arch = arch::x86_64) {
        bytecode_header header{};

        header.magic = OCCULT_MAGIC;
        header.version_major = OCCULT_VERSION_MAJOR;
        header.version_minor = OCCULT_VERSION_MINOR;
        header.target_arch = target_arch;
        header.header_size = sizeof(bytecode_header);
        header.code_offset = sizeof(bytecode_header); // code starts right after header
        // set const pool size and offset later on as well as code_size

        IRHash hash = hash_ir_function(func);
        header.ir_hash_low = hash.low64;
        header.ir_hash_high = hash.high64;

        return header;
    }

    void write_cached_code(const ir_function& func, const std::map<std::string, std::vector<std::uint8_t>>& function_raw_code_map, const std::unordered_map<std::string, jit_function>& function_map,
                           const std::unordered_map<std::uint64_t, std::string>& string_literals) {
        std::vector<std::uint8_t> serialized_string_map;
        std::vector<std::uint8_t> serialized_addr_map;

        auto header = generate_bytecode_header(func); // default to x86_64 for now
        header.const_pool_count = string_literals.size();
        auto code_it = function_raw_code_map.find(func.name);
        if (code_it == function_raw_code_map.end()) {
            throw std::runtime_error("write_cached_code: missing machine code for function " + func.name);
        }
        header.code_size = code_it->second.size();

        // layout for serialized addresses is the address 8 bytes followed by the
        // function name
        for (const auto& [name, fn] : function_map) {
            for (int i = 0; i < 8; ++i) {
                serialized_addr_map.push_back((reinterpret_cast<std::uint64_t>(fn) >> (i * 8)) & 0xFF);
            }

            for (auto& c : name) {
                serialized_addr_map.push_back(c);
            }

            serialized_addr_map.push_back('\0'); // end of string
        }

        // same layout as above, we just have the length as well, which is str - 8
        for (const auto& [address, literal] : string_literals) {
            for (int i = 0; i < 8; ++i) {
                serialized_string_map.push_back((reinterpret_cast<std::uint64_t>(address) >> (i * 8)) & 0xFF);
            }

            for (int i = 0; i < 8; ++i) { // str - 8 = length
                serialized_string_map.push_back((reinterpret_cast<std::uint64_t>(literal.length()) >> (i * 8)) & 0xFF);
            }

            for (auto& c : literal) {
                serialized_string_map.push_back(c);
            }

            serialized_string_map.push_back('\0');
        }

        /*
            layout:

            bytecode header
            machine code
            address_map
            const_pool
        */
        header.address_map_offset = header.code_offset + header.code_size; // beings right after
        header.const_pool_offset = header.address_map_offset + serialized_addr_map.size();

        std::vector<std::uint8_t> bytes_to_write;

        const auto* header_bytes = reinterpret_cast<const std::uint8_t*>(&header);
        bytes_to_write.insert(bytes_to_write.end(), header_bytes, header_bytes + sizeof(header));

        const auto& code = code_it->second;
        bytes_to_write.insert(bytes_to_write.end(), code.begin(), code.end());

        bytes_to_write.insert(bytes_to_write.end(), serialized_addr_map.begin(), serialized_addr_map.end());

        bytes_to_write.insert(bytes_to_write.end(), serialized_string_map.begin(), serialized_string_map.end());

        ensure_cache_dir();
        const std::string out_path = cache_path_for(func.name);
        std::ofstream out(out_path, std::ios::binary | std::ios::trunc);

        if (!out) {
            throw std::runtime_error("write_cached_code: failed to open " + out_path);
        }

        out.write(reinterpret_cast<const char*>(bytes_to_write.data()), static_cast<std::streamsize>(bytes_to_write.size()));
    }

    std::vector<std::uint8_t> load_cached_code(const std::string& func_name) {
        const std::string path = cache_path_for(func_name);
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file) {
            return {};
        }

        const std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::vector<std::uint8_t> bytes(size);
        if (!file.read(reinterpret_cast<char*>(bytes.data()), size)) {
            return {};
        }

        return bytes;
    }
} // namespace occult
