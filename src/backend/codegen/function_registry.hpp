#pragma once
#include "ir_gen.hpp"
#include "x86_64_codegen.hpp"
#include <tuple>
#include <typeinfo>
#include <unordered_map>

/*
    this entire file was heavily written with artificial intelligence, this shit is like wizardry and a pinnacle of "modern C++"
    i hate how this looks, and i can not wait to rewrite it from scratch in C++26 with PROPER REFLECTION! >:( 

    anyways, you can write C++ functions and register them to occult
    first you gotta register them to the IR, then the JIT runtime :D
*/

const std::unordered_map<std::string, std::string> cpp_to_occult_type_map = {
    {typeid(int).name(), "int32"},
    {typeid(long).name(), "int64"},
    {typeid(long long).name(), "int64"},
    {typeid(short).name(), "int16"},
    {typeid(char).name(), "int8"},
    {typeid(unsigned int).name(), "int32"},
    {typeid(unsigned long).name(), "int64"},
    {typeid(unsigned short).name(), "int16"},
    {typeid(unsigned char).name(), "int8"},
    {typeid(float).name(), "int32"},
    {typeid(double).name(), "int64"},
    {typeid(bool).name(), "int64"},
    {typeid(char *).name(), "str"}
};

namespace occult::function_registry {
    template<auto F>
    struct function_reflection_from_ptr;
}

#define OCCULT_FUNC_DECL(ret, name, params, ...) \
    ret name params; \
    template <> struct occult::function_registry::function_reflection_from_ptr<&name> { \
        using return_type = ret; \
        using args = std::tuple<__VA_ARGS__>; \
        static constexpr const char* name_str = #name; \
    }; \
    ret name params

#define OCCULT_FUNC_DECL_STATIC(ret, name, params, ...) \
    static ret name params; \
    template <> struct occult::function_registry::function_reflection_from_ptr<&name> { \
        using return_type = ret; \
        using args = std::tuple<__VA_ARGS__>; \
        static constexpr const char* name_str = #name; \
    }; \
    static ret name params

/*
How to use:
    OCCULT_FUNC_DECL(ReturnType, func_name, (Type param1, Type param2) Type, Type) { *Body* }
How to register:
    occult::function_registry::register_function_to_ir<&func_name>(<vec_of_ir_functions>); *This is first*
    occult::function_registry::register_function_to_codegen<&func_name>(<codegen_instance>); *This is second*
*/

namespace occult::function_registry {
    template<auto FuncPtr>
    void register_function_to_ir(std::vector<occult::ir_function> &ir_functions) {
        using reflection = function_reflection_from_ptr<FuncPtr>;
        occult::ir_function func;
        func.name = reflection::name_str;

        std::string return_type = typeid(typename reflection::return_type).name();
        auto it = cpp_to_occult_type_map.find(return_type);
        func.type = (it != cpp_to_occult_type_map.end()) ? it->second : "unknown";

        constexpr std::size_t N = std::tuple_size<typename reflection::args>::value;
        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            (([&] {
                using ArgType = std::tuple_element_t<Is, typename reflection::args>;
                std::string type_name = typeid(ArgType).name();
                auto type_it = cpp_to_occult_type_map.find(type_name);
                std::string occult_type = (type_it != cpp_to_occult_type_map.end()) ? type_it->second : "unknown";
                func.args.emplace_back("arg" + std::to_string(Is), occult_type);
            }()), ...);
        }(std::make_index_sequence<N>{});

        ir_functions.emplace_back(std::move(func));
    }

    template<auto FuncPtr, typename CodeGen>
    void register_function_to_codegen(CodeGen &codegen) {
        using reflection = function_reflection_from_ptr<FuncPtr>;
        codegen.function_map[reflection::name_str] = reinterpret_cast<jit_function>(FuncPtr);
    }
}
