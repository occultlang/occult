#pragma once
#include <string>
#include <iostream>
#include <ostream>
#include <fstream>
#include <sstream>

namespace occultlang 
{
    class convert_readable // class to convert a higher-level code to occult code (so i can add tuples and arrays easier?) also other things like tuples which i do plan to add
    {
        std::string generated;
        std::string default_code;
    public:
        convert_readable(const std::string& code) : default_code(code) {}

        std::string convert_to_readable();
    };
} // occultlang