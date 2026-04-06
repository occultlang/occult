#include "pe_header.hpp"

namespace occult {
    void generate_pe_header() {
        // TODO: add arguments for size, name, architecture, amount of sections, etc
        // etc

        IMAGE_DOS_HEADER dos_header;
        std::memset(&dos_header, 0, sizeof(dos_header));

        dos_header.e_magic = IMAGE_DOS_SIGNATURE; // "MZ"
        dos_header.e_lfanew = 0x80;               // placing the NT header at offset 0x80

        IMAGE_NT_HEADERS32 nt_headers;
        std::memset(&nt_headers, 0, sizeof(nt_headers));

        nt_headers.Signature = IMAGE_NT_SIGNATURE; // "PE\0\0"

        // file header
        nt_headers.FileHeader.Machine = IMAGE_FILE_MACHINE_I386;
        nt_headers.FileHeader.NumberOfSections = 1;
        // one section (.text), add an arugment for this later when we decide to add
        // more sections
        nt_headers.FileHeader.TimeDateStamp = 0; // not important
        nt_headers.FileHeader.PointerToSymbolTable = 0;
        nt_headers.FileHeader.NumberOfSymbols = 0;
        nt_headers.FileHeader.SizeOfOptionalHeader = sizeof(IMAGE_OPTIONAL_HEADER32);
        nt_headers.FileHeader.Characteristics = IMAGE_FILE_RELOCS_STRIPPED | IMAGE_FILE_EXECUTABLE_IMAGE | IMAGE_FILE_32BIT_MACHINE;

        // optional header (32-bit)
        nt_headers.OptionalHeader.Magic = IMAGE_NT_OPTIONAL_HDR32_MAGIC;
        nt_headers.OptionalHeader.MajorLinkerVersion = 1337;
        // magic number, highkey just change this shit towhatever you want
        nt_headers.OptionalHeader.MinorLinkerVersion = 0;

        nt_headers.OptionalHeader.SizeOfCode = 0x200; // 512 byte block of memory | TODO: change later on obviously
        nt_headers.OptionalHeader.SizeOfInitializedData = 0;
        nt_headers.OptionalHeader.SizeOfUninitializedData = 0;

        nt_headers.OptionalHeader.AddressOfEntryPoint = 0x1000; // RVA to our code
        nt_headers.OptionalHeader.BaseOfCode = 0x1000;
        nt_headers.OptionalHeader.BaseOfData = 0x2000; // not typically used, next alignment

        nt_headers.OptionalHeader.ImageBase = 0x00400000;    // typical default image base
        nt_headers.OptionalHeader.SectionAlignment = 0x1000; // section alignment
        nt_headers.OptionalHeader.FileAlignment = 0x0200;    // file alignment
        nt_headers.OptionalHeader.MajorOperatingSystemVersion = 4;
        nt_headers.OptionalHeader.MinorOperatingSystemVersion = 0;
        nt_headers.OptionalHeader.MajorImageVersion = 0;
        nt_headers.OptionalHeader.MinorImageVersion = 0;
        nt_headers.OptionalHeader.MajorSubsystemVersion = 4;
        nt_headers.OptionalHeader.MinorSubsystemVersion = 0;
        nt_headers.OptionalHeader.Win32VersionValue = 0;

        nt_headers.OptionalHeader.SizeOfImage = 0x2000;
        nt_headers.OptionalHeader.SizeOfHeaders = 0x200;
        nt_headers.OptionalHeader.CheckSum = 0;
        nt_headers.OptionalHeader.Subsystem = IMAGE_SUBSYSTEM_WINDOWS_CUI; // could change it to gui
        nt_headers.OptionalHeader.SizeOfStackReserve = 0x100000;
        nt_headers.OptionalHeader.SizeOfStackCommit = 0x1000;
        nt_headers.OptionalHeader.SizeOfHeapReserve = 0x100000;
        nt_headers.OptionalHeader.SizeOfHeapCommit = 0x1000;
        nt_headers.OptionalHeader.LoaderFlags = 0;
        nt_headers.OptionalHeader.NumberOfRvaAndSizes = IMAGE_NUMBEROF_DIRECTORY_ENTRIES;

        // minimal file, so no data directories are used, all 0
        // prepare section header for .text

        IMAGE_SECTION_HEADER section_header;
        std::memset(&section_header, 0, sizeof(section_header));
        std::memcpy(section_header.Name, ".text", 5);

        section_header.Misc.VirtualSize = 0x200;
        // typicall aligned to SectionAlignment, for minimal code lets just set it to
        // 0x200
        section_header.VirtualAddress = 0x1000; // in the loaded process .text is at ImageBase + 0x1000
        section_header.SizeOfRawData = 0x200;   // must be multiple of FileAlignment (0x200), we'll just use 0x200

        // PointerToRawData is where in the file section's bytes go
        // We put it at 0x200 because the first 512 bytes are for the DOS header +
        // stub + PE headers
        section_header.PointerToRawData = 0x200;
        section_header.PointerToRelocations = 0;
        section_header.PointerToLinenumbers = 0;
        section_header.NumberOfRelocations = 0;
        section_header.NumberOfLinenumbers = 0;
        section_header.Characteristics = IMAGE_SCN_CNT_CODE | IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_READ;

        unsigned char code[] = {0x31, 0xC0, 0xC3};
        std::size_t code_size = sizeof(code);

        std::ofstream file("min.exe", std::ios::binary);
        if (!file.good()) {
            std::cout << "failed to open min.exe for writing\n";
            return;
        }

        file.write(reinterpret_cast<const char*>(&dos_header), sizeof(dos_header));

        const char dos_stub[] = "This program cannot be run in DOS mode. \r\n$";
        file.write(dos_stub, sizeof(dos_stub) - 1);

        {
            // Pad up to 0x80 bytes total from start
            std::streamoff current_position = file.tellp();
            while (current_position < 0x80) {
                file.put('\0');
                current_position++;
            }
        }

        // NT Header
        file.write(reinterpret_cast<const char*>(&nt_headers), sizeof(nt_headers));

        // Section Header
        file.write(reinterpret_cast<const char*>(&section_header), sizeof(section_header));

        {
            // Pad until 0x200 (start of .text section) for headers
            std::streamoff current_position = file.tellp();
            while (current_position < 0x200) {
                file.put('\0');
                current_position++;
            }
        }

        // write .text section
        file.write(reinterpret_cast<const char*>(code), code_size);

        // Pad .text up to 0x200 bytes
        {
            std::streamoff current_position = file.tellp();
            while ((current_position - 0x200) < 0x200) {
                file.put('\0');
                current_position++;
            }
        }

        file.close();
        std::cout << "wrote successfully\n";
    }
}; // namespace occult
