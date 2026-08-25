#pragma once
#include "../../parser/cst.hpp"
#include "ir_defs.hpp"

#include <optional>
#include <stack>
#include <unordered_map>
#include <utility>
#include <variant>

namespace occult {
    struct visitor_stack {
        void operator()(const float& v) const { std::cout << v << "\n"; };
        void operator()(const double& v) const { std::cout << v << "\n"; };
        void operator()(const std::int64_t& v) const { std::cout << v << "\n"; };
        void operator()(const std::uint64_t& v) const { std::cout << v << "\n"; };
        void operator()(const std::int32_t& v) const { std::cout << v << "\n"; };
        void operator()(const std::uint32_t& v) const { std::cout << v << "\n"; };
        void operator()(const std::int16_t& v) const { std::cout << v << "\n"; };
        void operator()(const std::uint16_t& v) const { std::cout << v << "\n"; };
        void operator()(const std::int8_t& v) const { std::cout << static_cast<int>(v) << "\n"; };
        void operator()(const std::uint8_t& v) const { std::cout << static_cast<int>(v) << "\n"; };
        void operator()(const std::string& v) const { std::cout << v << "\n"; };
        void operator()(std::monostate) const { std::cout << "\n"; };
    };

    const std::unordered_map<std::string, ir_typename> ir_typemap = {
        {"int64", ir_typename::int64}, {"int32", ir_typename::int32},     {"int16", ir_typename::int16},     {"int8", ir_typename::int8},    {"uint64", ir_typename::uint64}, {"uint32", ir_typename::uint32}, {"uint16", ir_typename::uint16},
        {"uint8", ir_typename::uint8}, {"float32", ir_typename::float32}, {"float64", ir_typename::float64}, {"bool", ir_typename::boolean}, {"char", ir_typename::int8},     {"str", ir_typename::string},
    };

    static std::unordered_map<std::string, bool> is_signed = {
        {"int64", true}, {"int32", true}, {"int16", true}, {"int8", true}, {"uint64", false}, {"uint32", false}, {"uint16", false}, {"uint8", false}, {"bool", true}, {"char", true},
    };

    class ir_gen { // conversion into a linear IR
        cst_root* root;
        int label_count;
        std::stack<std::string> label_stack;
        std::unordered_map<std::string, int> label_map;
        bool debug;
        std::unordered_map<std::string, cst*> custom_type_map;
        std::unordered_map<std::string, ir_function> func_map;
        std::unordered_map<ir_function, std::unordered_map<std::string, std::string>,
                           ir_function_hasher> local_variable_map; // function -> variable name -> type

        std::unordered_map<ir_function, std::unordered_map<std::string, std::string>,
                           ir_function_hasher> local_array_map; // function -> array name -> type

        enum class type_of_push : std::uint8_t {
            normal, // normal push (more than one register)
            ret,    // pushes to return in codegen
            single  // pushes to a single register in codegen
        };

        ir_function generate_function(cst_function* func_node);

        void generate_function_args(ir_function& function, cst_functionargs* func_args_node);

        void generate_arith_and_bitwise_operators(ir_function& function, cst* c, std::optional<std::string> type = std::nullopt);

        void emit_comparison(ir_function& function, std::optional<std::string> type);

        void generate_boolean_value(ir_function& function, cst* node, type_of_push type_push = type_of_push::normal);

        template <typename IntType>
        void generate_common_generic(ir_function& function, cst* assignment_node, std::optional<std::string> type = std::nullopt, type_of_push type_push = type_of_push::normal);

        void handle_push_types(ir_function& function, cst* c, std::optional<std::string> type = std::nullopt);

        void generate_common(ir_function& function, cst* c, std::string type, type_of_push type_push = type_of_push::normal);

        void generate_function_call(ir_function& function, cst* c);

        void generate_return(ir_function& function, cst_returnstmt* return_node);

        void generate_or_jump(ir_function& function, cst* comparison, const std::string& true_label, bool is_float_cmp = false);

        void generate_and_jump(ir_function& function, cst* comparison, const std::string& false_label, bool is_float_cmp = false);

        void generate_inverted_jump(ir_function& function, cst* comparison, const std::string& false_label, bool is_float_cmp = false);

        void generate_normal_jump(ir_function& function, cst* comparison, const std::string& false_label, bool is_float_cmp = false);

        void generate_condition(ir_function& function, cst* node, const std::string& false_label, const std::string& true_label);

        void generate_if(ir_function& function, cst_ifstmt* if_node, const std::string& current_break_label = "", const std::string& current_loop_start = "");

        void generate_loop(ir_function& function, cst_loopstmt* loop_node);

        void generate_while(ir_function& function, cst_whilestmt* while_node);

        void generate_for(ir_function& function, cst_forstmt* for_node);

        void generate_switch(ir_function& function, cst_switchstmt* switch_node, const std::string& current_break_label = "", const std::string& current_loop_start = "");

        void generate_array_decl(ir_function& function, cst_array* array_node);

        void generate_array_access(ir_function& function, cst_arrayaccess* array_access_node);

        void generate_struct_decl(ir_function& function, cst_struct* struct_node);

        void generate_member_access(ir_function& function, cst_memberaccess* member_access_node);

        void generate_block(ir_function& function, cst_block* block_node, std::string current_break_label = "", std::string current_loop_start = "");

        std::string create_label();

        void place_label(ir_function& function, std::string label_name);

    public:
        ir_gen(cst_root* root, std::unordered_map<std::string, cst*> custom_type_map, const bool debug = false) : root(root), label_count(0), debug(debug), custom_type_map(std::move(custom_type_map)) {}

        static void visualize_stack_ir(const std::vector<ir_function>& funcs);

        static void visualize_structs(const std::vector<ir_struct>& structs);

        std::vector<ir_function> lower_functions();

        std::vector<ir_struct> lower_structs();
    };
} // namespace occult
