#include <iostream>
#include "code_generation/compiler.hpp"
#include "jit/tinycc_jit.hpp"
#include "static_analyzer/static_analyzer.hpp"
#include <thread>
#include "setup/setup.hpp"

int main(int argc, char *argv[]) 
{
    occultlang::setup setup;

    setup.setup_main(argc, argv);

    if (setup.help) 
    {
        return 0;
    }

    occultlang::static_analyzer analyzer{setup.source_original};

    analyzer.analyze();

    bool errors = !analyzer.get_error_list().empty();
    
    if (!errors) 
    {
        occultlang::compiler compiler{setup.source_original, setup.debug};

        std::string compiled = compiler.compile();

        occultlang::tinycc_jit jit{compiled, setup.output_file};

        if (setup.aot) {
            jit.run_aot();
        } else {
            jit.run();
        }

        return 0;
    }
    else 
    {
        std::cout << analyzer.get_error_list();
        return 1;
    }
}