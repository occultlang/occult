#pragma once
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>

extern "C" {
    #include "libtcc.h"
}

namespace occultlang 
{
    class tinycc_jit
    {
        TCCState* tcc;
        std::string source_original;
    public:
        tinycc_jit(const std::string& source) : source_original(source)
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