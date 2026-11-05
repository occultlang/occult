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
        std::int32_t hidden_slot_count = 0; // for __fn_result_<n> temps
        std::unordered_map<std::string, ir_function> func_map;
        std::unordered_map<ir_function, std::unordered_map<std::string, std::string>,
                           ir_function_hasher> local_variable_map; // function -> variable name -> type

        std::unordered_map<ir_function, std::unordered_map<std::string, std::string>,
                           ir_function_hasher> local_array_map; // function -> array name -> type

        // variables holding function pointers: function name -> var -> signature
        std::unordered_map<std::string, std::unordered_map<std::string, std::string>> fnptr_sigs;
        std::unordered_map<std::string, std::string> global_fnptr_sigs; // global var -> signature

        enum class type_of_push : std::uint8_t {
            normal, // normal push (more than one register)
            ret,    // pushes to return in codegen
            single  // pushes to a single register in codegen
        };

        ir_function generate_function(cst_function* func_node);

        void generate_function_args(ir_function& function, cst_functionargs* func_args_node);

        void generate_arith_and_bitwise_operators(ir_function& function, cst* c, std::optional<std::string> type = std::nullopt);

        void emit_comparison(ir_function& function, std::optional<std::string> type);
        // declared type of the last member in a.b.c
        std::optional<std::string> member_access_type(ir_function& function, cst* node);

        void generate_boolean_value(ir_function& function, cst* node, type_of_push type_push = type_of_push::normal);

        template <typename IntType>
        void generate_cast(ir_function& function, cst* c);

        template <typename IntType>
        void generate_common_generic(ir_function& function, cst* assignment_node, std::optional<std::string> type = std::nullopt, type_of_push type_push = type_of_push::normal);

        void handle_push_types(ir_function& function, cst* c, std::optional<std::string> type = std::nullopt);

        void generate_common(ir_function& function, cst* c, std::string type, type_of_push type_push = type_of_push::normal);

        void generate_function_call(ir_function& function, cst* c, bool result_used = true);
        void generate_call_itself(ir_function& function, cst* c, bool result_used);
        void generate_try(ir_function& function, cst* try_node, std::string current_break_label, std::string current_loop_start);
        void emit_defers(ir_function& function);
        void emit_defers_above(ir_function& function, std::size_t from_depth);
        // Set when a check in the function being generated propagates.
        bool propagates = false;
        std::vector<std::string> try_dispatch_stack;
        struct try_finally_context {
            std::string exit_var;
            std::string ret_var;
            std::string finally_label;
            std::string outer_break_label;
            std::string outer_loop_start;
            std::unordered_map<std::string, std::int64_t>* goto_codes; // name to dispatch code (4+)
        };
        std::vector<try_finally_context> try_finally_stack;
        std::vector<std::vector<cst*>> defer_bodies;
        std::vector<std::size_t> loop_defer_depths;

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

        std::string generate_lvalue_addr(ir_function& function, cst* node);

        static bool is_composite_access(cst* node);

        void emit_load_through_addr(ir_function& function, const std::string& type);

        void emit_store_through_addr(ir_function& function, cst* assignment_node, const std::string& type);

        static std::string member_cst_type_name(cst* member);

        void generate_block(ir_function& function, cst_block* block_node, std::string current_break_label = "", std::string current_loop_start = "");

        std::string create_label();

        // IR name of a label the source wrote, kept apart from label_<n>
        static std::string user_label(const std::string& name);

        void place_label(ir_function& function, std::string label_name);

    public:
        ir_gen(cst_root* root, std::unordered_map<std::string, cst*> custom_type_map, const bool debug = false) : root(root), label_count(0), debug(debug), custom_type_map(std::move(custom_type_map)) {}

        static void visualize_stack_ir(const std::vector<ir_function>& funcs);

        static void visualize_structs(const std::vector<ir_struct>& structs);

        std::vector<ir_function> lower_functions();

        std::vector<ir_struct> lower_structs();

        // top-level globals (name -> type), filled by lower_functions; codegen
        // gives these one shared storage slot instead of per-function locals
        std::vector<std::pair<std::string, std::string>> program_globals;
    };
} // namespace occult
