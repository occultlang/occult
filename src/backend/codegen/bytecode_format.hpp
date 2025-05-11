#pragma once

#include <stdint.h>

#define OCCULT_MAGIC 0x4F43434C  // get it, because it translates to "OCCL"... i don't know, i thought it was quite clever! ~nisan

#pragma pack(push, 1)
typedef struct {
    uint32_t magic;               // must be OCCULT_MAGIC
    uint16_t version_major;       // e.g. 2
    uint16_t version_minor;       // e.g. 0
    uint8_t  target_arch;         // 1 = x86, 2 = x64, etc.
    uint8_t  endianness;          // 0 = little, 1 = big
    uint16_t header_size;         // sizeof(occult_bytecode_header)
    uint32_t code_offset;         // from start of file to first instruction
    uint32_t code_size;           // total bytes of bytecode
    uint32_t const_pool_offset;   // file offset to constant pool
    uint32_t const_pool_count;    // number of entries in constant pool
    uint32_t entry_point;         // bytecode index (0-based) of initial instruction
} occult_bytecode_header;
#pragma pack(pop)
