#include "parser/parser.hpp"
#include <fstream>
#include <sstream>
#include "code_generation/code_gen.hpp"
#include <iostream>
#include <cstring> // for strcmp

extern "C" {
    #include "libtcc.h"
}

int main(int argc, char *argv[]) {
    bool debug = false;

    if (argc < 2 || argc > 3) {
        std::cerr << "usage: " << argv[0] << " <input_file> [-dbg]" << std::endl;
        return 1;
    }

    if (argc == 3 && std::string(argv[2]) == "-dbg") {
        debug = true;
    }

    std::string source_original;
    std::ifstream file(argv[1]);

    if (!file.is_open()) {
        std::cerr << "unable to open file: " << argv[1] << std::endl;
        return 1;
    }

    std::stringstream ss;
    std::string line;

    while (getline(file, line)) {
        ss << line << "\n";
    }

    source_original = ss.str();
    file.close();

    //std::cout << source_original << std::endl;

    occultlang::parser parser{ source_original };
    
    auto ast = parser.parse();

    if (debug)
        occultlang::ast::visualize(ast);

    // std::cout << std::endl;

    occultlang::code_gen code_gen;
    std::string generated = R"(
typedef struct dyn_array {
    union { // data types
        long* num;
    };
    
    int size;
} dyn_array;

dyn_array* create_array_long(int size) {
    dyn_array* array = (dyn_array*)malloc(sizeof(dyn_array));

    if (array == (void*)0) {
        exit(1);
    }

    array->size = size;
    array->num = (long*)malloc(size * sizeof(long));

    if (array->num == (void*)0) {
        free(array);
        exit(1);
    }

    return array;
}

dyn_array* create_array() {
    return create_array_long(0);
}

// add unsigned long

void add_long(dyn_array* array, long data) {
    if (array->num == (void*)0) {
        exit(1);
    }

    array->size++;
    array->num = (long*)realloc(array->num, array->size * sizeof(long));

    if (array->num == (void*)0) {
        exit(1);
    }

    array->num[array->size - 1] = data;

    printf("new size: %i\n", array->size);
}

void delete_array(dyn_array* array) {
    free(array->num);
    free(array);
}
    )";

    generated += code_gen.compile<occultlang::ast>(ast, debug, occultlang::debug_level::all);

    if (debug)
        std::cout << std::endl << generated << std::endl << std::endl;

    TCCState* tcc = tcc_new();

    if (!tcc) {
        std::cerr << "failed to create tcc state" << std::endl;
        return 1;
    }

    tcc_add_library_path(tcc, "./");

    tcc_set_options(tcc, "-g -w");

    tcc_set_output_type(tcc, TCC_OUTPUT_MEMORY);

    if (tcc_compile_string(tcc, generated.c_str()) == -1) {
        std::cerr << "failed to compile code" << std::endl;
        return 1;
    }

    tcc_run(tcc, 0, 0);     

    tcc_delete(tcc);

    return 0;
}
