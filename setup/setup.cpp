#include "setup.hpp"

namespace occultlang 
{
    void setup::setup_main(int& argc, char **(&argv))
    {
        for (int i = 1; i < argc; i++)
        {
            if (std::string(argv[i]) == "-d")
            {
                debug = true;
            }
            else if (std::string(argv[i]) == "-o")
            {
                aot = true;
                output_file = argv[i + 1];
                i++;
            }
            else if (std::string(argv[i]) == "-h")
            {
                help = true;
            }
            else
            {
                input_file = argv[i];
            }
        }

        if (input_file.empty())
        {
            std::cout << "No input file specified" << std::endl;
            help = true;
        }

        if (help)
        {
            std::cout << "Usage: occult [options] <input_file>" << std::endl;
            std::cout << "Options:" << std::endl;
            std::cout << "-d: debug mode" << std::endl;
            std::cout << "-o <output_file>: specify output file" << std::endl;
            return;
        }

        std::ifstream file(input_file);
        std::stringstream buffer;
        buffer << file.rdbuf();
        source_original = buffer.str();
    }
} // occultlang

