#pragma once
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>

namespace occultlang 
{
    struct setup 
    {        
        bool help = false;
        bool debug = false;
        bool aot = false;
        std::string output_file = "a.out";
        std::string input_file;
        std::string source_original;
        setup() {}
        void setup_main(int& argc, char **(&argv));
    };
} // occultlang