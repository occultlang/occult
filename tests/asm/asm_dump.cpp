// Assembles each line of a file (or one line per argv) with Occult's assembler and prints hex.
// usage: asm_dump <file>   -- one instruction per line; prints "<hex>" or "ERR: msg" per line
#include <fstream>
#include <iostream>
#include <sstream>
#include "../../src/backend/codegen/x86_64_assembler.hpp"

int main(int argc, char** argv) {
    if (argc < 2) return 2;
    std::ifstream f(argv[1]);
    std::string line;
    int rc = 0;
    while (std::getline(f, line)) {
        try {
            occult::x86_64::assembler a(line, {}, [](const std::string&) { return false; });
            auto out = a.assemble();
            for (auto b : out.bytes) printf("%02x", b);
            printf("\n");
        }
        catch (const std::exception& e) {
            printf("ERR: %s\n", e.what());
            rc = 1;
        }
    }
    return rc;
}
