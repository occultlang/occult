#pragma once

#include <cstdint>

#pragma pack(push, 1)
namespace occult {
constexpr std::uint32_t OCCULT_MAGIC = 0x4F43434C;
// get it, because it translates to "OCCL"... i don't know, i thought it was
// quite clever! ~nisan
constexpr std::uint16_t OCCULT_VERSION_MAJOR = 2;
constexpr std::uint16_t OCCULT_VERSION_MINOR = 2;

enum class arch : std::uint8_t {
  x86_64 = 54,
  riscv64 = 18,
  arm64 = 98
}; // random

typedef struct {
  std::uint32_t magic;             // must be OCCULT_MAGIC
  std::uint16_t version_major;     // e.g. 2
  std::uint16_t version_minor;     // e.g. 0
  arch target_arch;                // see arch enumeration
  std::uint16_t header_size;       // sizeof(occult_bytecode_header)
  std::uint32_t code_offset;       // from start of file to first instruction
  std::size_t code_size;           // total bytes of bytecode
  std::uint64_t const_pool_offset; // file offset to constant pool
  std::uint32_t const_pool_count;  // number of entries in constant pool
  // std::uint32_t entry_point;      // bytecode index (0-based) of initial
  // instruction
  std::uint64_t address_map_offset; // used for reloc addresses
  std::uint64_t ir_hash_low;        // start of 128 bit hash (low bits)
  std::uint64_t ir_hash_high;       // high bits of hash
} bytecode_header;                  // we will by default use little endian
} // namespace occult
#pragma pack(pop)
