#include "tinycc_jit.hpp"

namespace occultlang 
{
    void tinycc_jit::run()
    {
        if (!tcc) {
            std::cerr << "failed to create tcc state" << std::endl;
            return;
        }

        tcc_add_library_path(tcc, "./");

        tcc_set_options(tcc, "-g -w");

        tcc_set_output_type(tcc, TCC_OUTPUT_MEMORY);

        if (tcc_compile_string(tcc, source_original.c_str()) == -1) {
            std::cerr << "failed to compile code" << std::endl;
            return;
        }

        tcc_run(tcc, 0, 0);
    }

    void tinycc_jit::run_aot()
    {
        if (!tcc) {
            std::cerr << "failed to create tcc state" << std::endl;
            return;
        }

        tcc_add_library_path(tcc, "./");

        tcc_set_options(tcc, "-g -w");

        tcc_set_output_type(tcc, TCC_OUTPUT_EXE);

        if (tcc_compile_string(tcc, source_original.c_str()) == -1) {
            std::cerr << "failed to compile code" << std::endl;
            return;
        }

        tcc_output_file(tcc, "occult_out");
    }
} // occultlang