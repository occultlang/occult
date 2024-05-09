#pragma once
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>

extern "C" {
    #include "libtcc.h"
}

namespace occultlang 
{
    class tinycc_jit
    {
        TCCState* tcc;
        std::string source_original;
        std::string filename;
    public:
        tinycc_jit(const std::string& source, std::string filename) : source_original(source), filename(filename) 
        {
            tcc = tcc_new();
        }

        ~tinycc_jit()
        {
            tcc_delete(tcc);
        }
        
        void run();
        void run_aot();
    };
} // occultlang