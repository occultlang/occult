#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace occult {
    namespace pedef {
        // https://learn.microsoft.com/windows/win32/debug/pe-format
        constexpr std::uint16_t DOS_SIGNATURE = 0x5A4D;   // MZ
        constexpr std::uint32_t NT_SIGNATURE = 0x00004550; // PE\0\0
        constexpr std::uint16_t FILE_MACHINE_AMD64 = 0x8664;
        constexpr std::uint16_t NT_OPTIONAL_HDR64_MAGIC = 0x020B;

        constexpr std::uint16_t FILE_RELOCS_STRIPPED = 0x0001;
        constexpr std::uint16_t FILE_EXECUTABLE_IMAGE = 0x0002;
        constexpr std::uint16_t FILE_LARGE_ADDRESS_AWARE = 0x0020;
        constexpr std::uint16_t FILE_DLL = 0x2000;

        constexpr std::uint16_t SUBSYSTEM_WINDOWS_CUI = 3;

        constexpr std::uint32_t SCN_CNT_CODE = 0x00000020;
        constexpr std::uint32_t SCN_CNT_INITIALIZED_DATA = 0x00000040;
        constexpr std::uint32_t SCN_MEM_DISCARDABLE = 0x02000000;
        constexpr std::uint32_t SCN_MEM_EXECUTE = 0x20000000;
        constexpr std::uint32_t SCN_MEM_READ = 0x40000000;
        constexpr std::uint32_t SCN_MEM_WRITE = 0x80000000;

        constexpr std::size_t DIRECTORY_ENTRY_EXPORT = 0;
        constexpr std::size_t DIRECTORY_ENTRY_IMPORT = 1;
        constexpr std::size_t DIRECTORY_ENTRY_BASERELOC = 5;
        constexpr std::size_t DIRECTORY_ENTRY_IAT = 12;
        constexpr std::size_t NUMBEROF_DIRECTORY_ENTRIES = 16;

        struct dos_header {
            std::uint16_t e_magic;
            std::uint16_t e_cblp, e_cp, e_crlc, e_cparhdr;
            std::uint16_t e_minalloc, e_maxalloc, e_ss, e_sp, e_csum;
            std::uint16_t e_ip, e_cs, e_lfarlc, e_ovno;
            std::uint16_t e_res[4];
            std::uint16_t e_oemid, e_oeminfo;
            std::uint16_t e_res2[10];
            std::uint32_t e_lfanew;
        };

        struct file_header {
            std::uint16_t Machine;
            std::uint16_t NumberOfSections;
            std::uint32_t TimeDateStamp;
            std::uint32_t PointerToSymbolTable;
            std::uint32_t NumberOfSymbols;
            std::uint16_t SizeOfOptionalHeader;
            std::uint16_t Characteristics;
        };

        struct data_directory {
            std::uint32_t VirtualAddress;
            std::uint32_t Size;
        };

        struct optional_header64 {
            std::uint16_t Magic;
            std::uint8_t MajorLinkerVersion, MinorLinkerVersion;
            std::uint32_t SizeOfCode, SizeOfInitializedData, SizeOfUninitializedData;
            std::uint32_t AddressOfEntryPoint, BaseOfCode;
            std::uint64_t ImageBase;
            std::uint32_t SectionAlignment, FileAlignment;
            std::uint16_t MajorOperatingSystemVersion, MinorOperatingSystemVersion;
            std::uint16_t MajorImageVersion, MinorImageVersion;
            std::uint16_t MajorSubsystemVersion, MinorSubsystemVersion;
            std::uint32_t Win32VersionValue;
            std::uint32_t SizeOfImage, SizeOfHeaders, CheckSum;
            std::uint16_t Subsystem, DllCharacteristics;
            std::uint64_t SizeOfStackReserve, SizeOfStackCommit;
            std::uint64_t SizeOfHeapReserve, SizeOfHeapCommit;
            std::uint32_t LoaderFlags, NumberOfRvaAndSizes;
            data_directory DataDirectory[NUMBEROF_DIRECTORY_ENTRIES];
        };

        struct section_header {
            char Name[8];
            std::uint32_t VirtualSize, VirtualAddress;
            std::uint32_t SizeOfRawData, PointerToRawData;
            std::uint32_t PointerToRelocations, PointerToLinenumbers;
            std::uint16_t NumberOfRelocations, NumberOfLinenumbers;
            std::uint32_t Characteristics;
        };

        struct import_descriptor {
            std::uint32_t OriginalFirstThunk;
            std::uint32_t TimeDateStamp;
            std::uint32_t ForwarderChain;
            std::uint32_t Name;
            std::uint32_t FirstThunk;
        };

        static_assert(sizeof(dos_header) == 64);
        static_assert(sizeof(file_header) == 20);
        static_assert(sizeof(optional_header64) == 240);
        static_assert(sizeof(section_header) == 40);
        static_assert(sizeof(import_descriptor) == 20);
    } // namespace pedef

    struct pe {
        static constexpr std::uint32_t section_align = 0x1000;
        static constexpr std::uint32_t file_align = 0x200;
        static constexpr std::uint32_t text_rva = 0x1000;
        static constexpr std::uint64_t image_base = 0x140000000ULL;

        // one DLL, and the names wanted from it
        struct import {
            std::string dll;
            std::vector<std::string> names;
        };

        using iat_map = std::unordered_map<std::string, std::uint32_t>;

        static std::vector<std::uint8_t> build_imports(const std::vector<import>& imports,
                                                       std::uint32_t at_rva,
                                                       iat_map& iat,
                                                       pedef::data_directory& import_dir,
                                                       pedef::data_directory& iat_dir);

        static void generate_binary(const std::string& binary_name,
                                    const std::vector<std::uint8_t>& image,
                                    std::uint32_t entry_rva,
                                    const pedef::data_directory& import_dir,
                                    const pedef::data_directory& iat_dir);

        struct export_entry {
            std::string name;
            std::uint32_t rva;
        };

        static std::vector<std::uint8_t> build_exports(const std::vector<export_entry>& exports,
                                                       const std::string& dll_name,
                                                       std::uint32_t at_rva,
                                                       pedef::data_directory& export_dir);

        static void generate_dll(const std::string& binary_name,
                                 const std::vector<std::uint8_t>& image,
                                 std::uint32_t entry_rva,
                                 const pedef::data_directory& import_dir,
                                 const pedef::data_directory& iat_dir,
                                 const pedef::data_directory& export_dir,
                                 const std::vector<std::size_t>& abs_relocs);
    };
} // namespace occult
