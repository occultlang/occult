#include "parser/parser.hpp"
#include <fstream>
#include <sstream>
#include "code_generation/code_gen.hpp"
#include <iostream>

extern "C" {
    #include "libtcc.h"
}

int main(int argc, char *argv[]) {
    bool debug = false;
    bool aot = false;

    if (argc < 2 || argc > 4) {
        std::cerr << "usage: " << argv[0] << " <input_file> [-aot] [-dbg]" << std::endl;
        return 1;
    }

     for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-aot") {
            aot = true;
        } else if (arg == "-dbg") {
            debug = true;
        }
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
        double* rnum;
        const char* str;
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

dyn_array* create_array_double(int size) {
    dyn_array* array = (dyn_array*)malloc(sizeof(dyn_array));

    if (array == (void*)0) {
        exit(1);
    }

    array->size = size;
    array->rnum = (double*)malloc(size * sizeof(double));

    if (array->rnum == (void*)0) {
        free(array);
        exit(1);
    }

    return array;
}

dyn_array* create_array_string(int size) {
    dyn_array* array = (dyn_array*)malloc(sizeof(dyn_array));

    if (array == (void*)0) {
        exit(1);
    }

    array->size = size;
    array->str = (const char**)malloc(size * sizeof(const char*));

    if (array->str == (void*)0) {
        free(array);
        exit(1);
    }

    return array;
}

int get_size(dyn_array* array) {
    return array->size;
}

long get_long(dyn_array* array, int index) {
    if (index < 0 || index >= array->size) {
        exit(1);
    }

    return array->num[index];
}

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
}

void set_long(dyn_array* array, int index, long data) {
    if (index < 0 || index >= array->size) {
        exit(1);
    }

    array->num[index] = data;
}

double get_double(dyn_array* array, int index) {
    if (index < 0 || index >= array->size) {
        exit(1);
    }

    return array->rnum[index];
}

void add_double(dyn_array* array, double data) {
    if (array->rnum == (void*)0) {
        exit(1);
    }

    array->size++;
    array->rnum = (double*)realloc(array->rnum, array->size * sizeof(double));

    if (array->rnum == (void*)0) {
        exit(1);
    }

    array->rnum[array->size - 1] = data;
}

void set_double(dyn_array* array, int index, double data) {
    if (index < 0 || index >= array->size) {
        exit(1);
    }

    array->rnum[index] = data;
}

const char* get_string(dyn_array* array, int index) {
    if (index < 0 || index >= array->size) {
        exit(1);
    }

    return array->str[index];
}

void add_string(dyn_array* array, const char* data) {
    if (array->str == (void*)0) {
        exit(1);
    }

    array->size++;
    array->str = (const char**)realloc(array->str, array->size * sizeof(const char*));

    if (array->str == (void*)0) {
        exit(1);
    }

    array->str[array->size - 1] = data;
}

void set_string(dyn_array* array, int index, const char* data) {
    if (index < 0 || index >= array->size) {
        exit(1);
    }

    array->str[index] = data;
}

void delete_array(dyn_array* array) {
    free(array->num);
    free(array->rnum);
    free(array->str);
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
