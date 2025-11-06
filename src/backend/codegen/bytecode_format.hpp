#pragma once

#include <cstdint>

#pragma pack(push, 1)
namespace occult {
    constexpr std::uint32_t OCCULT_MAGIC = 0x4F43434C;
    // get it, because it translates to "OCCL"... i don't know, i thought it was quite clever! ~nisan
    constexpr std::uint16_t OCCULT_VERSION_MAJOR = 2;
    constexpr std::uint16_t OCCULT_VERSION_MINOR = 0;

    enum class arch : std::uint8_t {
        x86_64,
        riscv64,
        arm64
    };

    typedef struct {
        std::uint32_t magic; // must be kOCCULTMagic
        std::uint16_t version_major; // e.g. 2
        std::uint16_t version_minor; // e.g. 0
        arch target_arch; // see arch enumeration
        std::uint16_t header_size; // sizeof(occult_bytecode_header)
        std::uint32_t code_offset; // from start of file to first instruction
        std::uint32_t code_size; // total bytes of bytecode
        std::int32_t const_pool_offset; // file offset to constant pool
        std::uint32_t const_pool_count; // number of entries in constant pool
        std::uint32_t entry_point; // bytecode index (0-based) of initial instruction
    } bytecode_header; // we will by default use little endian
}
#pragma pack(pop)
