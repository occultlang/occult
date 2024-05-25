#include "finder.hpp"

namespace occultlang  // thank copilot so much for helping with this regex code
{
    std::string finder::match_and_replace_all_array(std::string source, std::string to_replace)
    {
        std::regex pattern("(array|array:)(\\s+\\w+\\s*=\\s*\\[)");
        std::string new_source = std::regex_replace(source, pattern, to_replace + "$2");
        return new_source;  
    }

    std::string finder::match_and_replace_array_more(std::string source, std::string to_replace) 
    {
        std::regex pattern("(array)(\\s+\\w+\\s*)");
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
    
    std::string finder::match_and_replace_casts(std::string source)
    {
        std::regex cast_pattern("(deref )?(\\w+)( as )(num ptr|rnum ptr|string ptr|void ptr|num|rnum|string)");
        std::string result;
        std::smatch m;
        std::string::const_iterator search_start(source.cbegin());

        while (std::regex_search(search_start, source.cend(), m, cast_pattern)) {
            std::string deref = m[1].str();
            std::string type = m[4].str();
            std::string replacement;
            if (type == "num ptr") replacement = "(long deref)" + m[2].str();
            else if (type == "rnum ptr") replacement = "(double deref)" + m[2].str();
            else if (type == "string ptr") replacement = "(char deref deref)" + m[2].str();
            else if (type == "void ptr") replacement = "(void deref)" + m[2].str();
            else if (type == "num") replacement = "(long)" + m[2].str();
            else if (type == "rnum") replacement = "(double)" + m[2].str();
            else if (type == "string") replacement = "(char deref)" + m[2].str();
            else replacement = m[0].str(); // append the original match if the type is not recognized

            if (!deref.empty()) replacement = "deref" + replacement; // add dereference operator if 'deref' was present

            result += source.substr(search_start - source.cbegin(), m.position()) + replacement; // append the text before the match and the replacement to the result
            search_start = m.suffix().first;
        }

        result += source.substr(search_start - source.cbegin()); // append the remaining text after the last match

        return result;
    }

    std::string finder::remove_lonely_semicolons(std::string source)
    {
        std::regex pattern(R"(\n;\n)"); 
        std::string new_source = std::regex_replace(source, pattern, "\n"); 
        return new_source;  
    }

    /* we need a function that can recursively flatten the jagged arrays into different sub arrays + generate using only flat arrays */
} // occultlang