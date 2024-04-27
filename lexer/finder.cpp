#include "finder.hpp"

namespace occultlang 
{
    std::string finder::match_and_replace_all_array(std::string source, std::string to_replace)
    {
        std::regex pattern("(array:)(\\s+\\w+\\s*=\\s*\\[)");
        std::string new_source = std::regex_replace(source, pattern, to_replace + "$2");
        return new_source;  
    }

    std::string finder::match_and_replace_all(std::string source, std::string to_find, std::string to_replace)
    {
        std::size_t pos = 0;

        while ((pos = source.find(to_find, pos)) != std::string::npos) 
        {
            source.replace(pos, to_find.length(), to_replace);
            pos += to_replace.length();
        }

        return source;
    }
} // occultlang