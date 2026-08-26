#pragma once
#include <variant>

namespace occult {
    constexpr const char* kStructPtrSuffix = "_ptr";

    enum ir_opcode {
        null_op,
        op_push,
        op_push_for_ret,
        op_push_single,
        // op_pushf,
        op_store,
        op_load,
        /*op_storef32,
        op_loadf32,
        op_storef64,
        op_loadf64,*/
        op_add,
        op_div,
        op_mod,
        op_sub,
        op_mul,
        op_imul,
        op_idiv,
        op_imod,
        op_logical_and,
        op_logical_or,
        op_bitwise_and,
        op_bitwise_or,
        op_bitwise_xor,
        op_bitwise_not,
        op_not,
        op_bitwise_lshift,
        op_bitwise_rshift,
        op_ibitwise_rshift,
        op_negate,
        op_negatef32,
        op_negatef64,
        op_jmp,
        op_jz,  // je
        op_jnz, // jne
        op_jl,
        op_jle,
        op_jg,
        op_jge,
        op_jb,  // unsigned less than (for float comparisons after COMISS/COMISD)
        op_jbe, // unsigned less than or equal
        op_ja,  // unsigned greater than
        op_jae, // unsigned greater than or equal
        op_setz,
        op_setnz,
        op_setl,
        op_setle,
        op_setg,
        op_setge,
        op_cmp,
        op_cmpf32,
        op_cmpf64,
        op_ret,
        op_call,
        op_syscall,
        op_addf32,
        op_divf32,
        op_subf32,
        op_mulf32,
        op_modf32,
        op_addf64,
        op_divf64,
        op_subf64,
        op_mulf64,
        op_modf64,
        label,
        op_array_decl,
        op_array_access_element,
        op_array_store_element,
        op_declare_where_to_store,
        op_array_dimensions,
        op_array_size,
        op_decl_array_type,
        op_reference,
        op_dereference,
        op_dereference_assign,
        op_store_at_addr,
        op_mark_for_array_access,
        op_struct_decl,
        op_member_access,
        op_member_store,
        op_struct_load,
        op_struct_store,
        op_push_shellcode,
        op_cast,
        op_bitcast,
        op_asm_code
    };

    inline std::string opcode_to_string(ir_opcode op) {
        switch (op) {
        case op_push:
            return "push";
        // case op_pushf:
        //     return "pushf";
        case op_store:
            return "store";
        case op_load:
            return "load";
        /*case op_loadf32:
            return "loadf32";
        case op_storef32:
            return "storef32";
        case op_loadf64:
            return "loadf64";
        case op_storef64:
            return "storef64";*/
        case op_add:
            return "add";
        case op_div:
            return "div";
        case op_mod:
            return "mod";
        case op_sub:
            return "sub";
        case op_mul:
            return "mul";
        case op_imul:
            return "imul";
        case op_idiv:
            return "idiv";
        case op_imod:
            return "imod";
        case op_logical_and:
            return "logical_and";
        case op_logical_or:
            return "logical_or";
        case op_bitwise_and:
            return "bitwise_and";
        case op_bitwise_or:
            return "bitwise_or";
        case op_bitwise_xor:
            return "bitwise_xor";
        case op_bitwise_not:
            return "bitwise_not";
        case op_not:
            return "not";
        case op_cast:
            return "cast";
        case op_bitcast:
            return "bitcast";
        case op_bitwise_lshift:
            return "bitwise_lshift";
        case op_bitwise_rshift:
            return "bitwise_rshift";
        case op_ibitwise_rshift:
            return "ibitwise_rshift";
        case op_negate:
            return "negate";
        case op_negatef32:
            return "negatef32";
        case op_negatef64:
            return "negatef64";
        case op_jmp:
            return "jmp";
        case op_jz:
            return "jz";
        case op_jnz:
            return "jnz";
        case op_jl:
            return "jl";
        case op_jle:
            return "jle";
        case op_jg:
            return "jg";
        case op_jge:
            return "jge";
        case op_jb:
            return "jb";
        case op_jbe:
            return "jbe";
        case op_ja:
            return "ja";
        case op_jae:
            return "jae";
        case op_setz:
            return "setz";
        case op_setnz:
            return "setnz";
        case op_setl:
            return "setl";
        case op_setle:
            return "setle";
        case op_setg:
            return "setg";
        case op_setge:
            return "setge";
        case op_cmp:
            return "cmp";
        case op_cmpf32:
            return "cmpf32";
        case op_cmpf64:
            return "cmpf64";
        case op_ret:
            return "ret";
        case op_call:
            return "call";
        case op_syscall:
            return "syscall";
        case op_addf32:
            return "addf32";
        case op_addf64:
            return "addf64";
        case op_divf32:
            return "divf32";
        case op_divf64:
            return "divf64";
        case op_subf32:
            return "subf32";
        case op_subf64:
            return "subf64";
        case op_mulf32:
            return "mulf32";
        case op_mulf64:
            return "mulf64";
        case op_modf32:
            return "modf32";
        case op_modf64:
            return "modf64";
        case label:
            return "label";
        case op_array_decl:
            return "array_decl";
        case op_array_access_element:
            return "array_access_elem";
        case op_array_store_element:
            return "array_store_element";
        case op_array_dimensions:
            return "array_dimensions";
        case op_array_size:
            return "array_size";
        case op_decl_array_type:
            return "decl_array_type";
        case op_declare_where_to_store:
            return "declare_where_to_store";
        case op_reference:
            return "reference";
        case op_dereference:
            return "dereference";
        case op_dereference_assign:
            return "dereference_assign";
        case op_store_at_addr:
            return "store_at_addr";
        case op_mark_for_array_access:
            return "mark_for_array_access";
        case op_struct_decl:
            return "struct_decl";
        case op_member_access:
            return "member_access";
        case op_member_store:
            return "member_store";
        case op_struct_load:
            return "struct_load";
        case op_struct_store:
            return "struct_store";
        case op_push_for_ret:
            return "push_ret";
        case op_push_single:
            return "push_single";
        case op_push_shellcode:
            return "push_shellcode";
        case op_asm_code:
            return "asm_code";
        default:
            return "unknown_opcode";
        }
    }

    struct ir_argument {
        std::string name;
        std::string type;

        ir_argument(std::string name, std::string type) : name(std::move(name)), type(std::move(type)) {}
    };

    using ir_operand = std::variant<std::monostate, std::int64_t, std::uint64_t, std::int32_t, std::uint32_t, std::int16_t, std::uint16_t, std::int8_t, std::uint8_t, double, float, std::string>;

    struct ir_instr {
        ir_opcode op;
        ir_operand operand;
        std::string type;

        ir_instr(const ir_opcode op, ir_operand operand) : op(op), operand(std::move(operand)) {}

        ir_instr(const ir_opcode op, ir_operand operand, std::string type) : op(op), operand(std::move(operand)), type(std::move(type)) {}

        explicit ir_instr(const ir_opcode op) : op(op), operand(std::monostate()) {}
    };

    using ir_body = std::vector<ir_instr>;

    struct ir_function {
        ir_body code;
        std::vector<ir_argument> args;
        std::string name;
        std::string type;
        bool uses_shellcode = false;
        bool is_external = false;
        bool is_variadic = false;
        bool uses_assembly = false;

        bool operator==(const ir_function& other) const { return name == other.name; }
    };

    struct ir_function_hasher {
        size_t operator()(const ir_function& ir_func) const { return std::hash<std::string>{}(ir_func.name); }
    };

    struct ir_struct_member {
        std::string datatype; // name of the datatype / or structure for custom datatype
        std::string name;     // name of the member variable

        ir_struct_member() = default;

        ir_struct_member(std::string datatype, std::string name) : datatype(std::move(datatype)), name(std::move(name)) {}
    };

    struct ir_struct {                         // used for custom data types (structures in the IR)
        std::string datatype;                  // name of the structure / custom data type
        std::vector<ir_struct_member> members; // list of members

        ir_struct() = default;

        ir_struct(std::string datatype, std::vector<ir_struct_member> members) : datatype(std::move(datatype)), members(std::move(members)) {}
    };

    enum ir_typename : std::uint8_t { int8, int16, int32, int64, uint8, uint16, uint32, uint64, float32, float64, string, boolean };

    // start of register ir (rir)

    enum rir_opcode {
        rop_null,
        rop_mov,
    };

    struct rir_vreg {
        std::uint32_t id;
        bool operator==(const rir_vreg& o) const { return id == o.id; }
    };

    using rir_operand = std::variant<std::monostate, rir_vreg, std::int64_t, std::uint64_t, std::int32_t, std::uint32_t, std::int16_t, std::uint16_t, std::int8_t, std::uint8_t, double, float, std::string>;

    struct rir_instr {
        rir_opcode op;
        ir_vreg dst{};                    
        rir_operand src[2]{};            
        std::vector<rir_operand> extra;   // only populated for call args / other variadic ops
        std::string type; 

        rir_instr() : op(rop_null) {}

        rir_instr(rir_opcode op, ir_vreg dst) : op(op), dst(dst) {}

        rir_instr(rir_opcode op, ir_vreg dst, rir_operand s0) : op(op), dst(dst) { src[0] = std::move(s0); }

        rir_instr(rir_opcode op, ir_vreg dst, rir_operand s0, rir_operand s1) : op(op), dst(dst) {
            src[0] = std::move(s0);
            src[1] = std::move(s1);
        }

        rir_instr(rir_opcode op, ir_vreg dst, rir_operand s0, std::string type)
            : op(op), dst(dst), type(std::move(type)) {
            src[0] = std::move(s0);
        }
    };

    using rir_body = std::vector<rir_instr>;

    struct rir_function {
        rir_body code;
        std::vector<ir_argument> args;
        std::string name;
        std::string type;
        bool uses_shellcode = false;
        bool is_external = false;
        bool is_variadic = false;
        bool uses_assembly = false;
        std::uint32_t vreg_count = 0; // how many vregs this function allocated
    };

    struct vreg_allocator {
        std::uint32_t next = 0;
        rir_vreg fresh() { return rir_vreg{ next++ }; }
    };
} // namespace occult