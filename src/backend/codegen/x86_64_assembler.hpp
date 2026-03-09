#pragma once
#include "x86_64_writer.hpp"
#include "x86_64_assembler_defs.hpp"
#include "../../lexer/lexer_maps.hpp" // for whitespace map
#include "../../lexer/number_parser.hpp"

// used for a subset of x86_64 inline assembly in occult

namespace occult::x86_64 {
    class assembler {
        std::vector<assembler_token> assembler_token_vec;
        x86_64_writer w;
        std::string source;
        std::size_t pos = 0;
        std::size_t pos_stream = 0;

        void handle_whitespace() {
            while (pos < source.length() && whitespace_map.contains(source[pos])) {
                ++pos;
            }
        }

        assembler_token handle_numeric() {
            if (source[pos] == '0') {
                pos += 1;

                if (source[pos] == 'x' || source[pos] == 'X') {
                    pos += 1;

                    std::string hex_number;
                    while (is_hex(source[pos])) {
                        hex_number += source[pos];
                        pos += 1;
                    }

                    if (hex_number.empty()) {
                        return assembler_token("0", x86_64_instr_token::number_literal_tt);
                    }

                    try {
                        hex_number = to_parsable_type<std::uintptr_t>(hex_number, HEX_BASE);
                    }
                    catch (const std::exception&) {
                        return assembler_token("0", x86_64_instr_token::number_literal_tt);
                    }
                    return assembler_token(hex_number, x86_64_instr_token::number_literal_tt);
                }

                if (source[pos] == 'b' || source[pos] == 'B') {
                    pos += 1;

                    std::string binary_number;
                    while (is_binary(source[pos])) {
                        binary_number += source[pos];
                        pos += 1;
                    }

                    if (binary_number.empty()) {
                        return assembler_token("0", x86_64_instr_token::number_literal_tt);
                    }

                    try {
                        binary_number = to_parsable_type<std::uintptr_t>(binary_number, BINARY_BASE);
                    }
                    catch (const std::exception&) {
                        return assembler_token("0", x86_64_instr_token::number_literal_tt);
                    }
                    return assembler_token(binary_number, x86_64_instr_token::number_literal_tt);
                }

                if (source[pos] != '.') {
                    std::uintptr_t octal_value = 0;
                    while (is_octal(source[pos])) {
                        octal_value = octal_value * OCTAL_BASE + (source[pos] - '0');
                        pos += 1;
                    }

                    return assembler_token(std::to_string(octal_value), x86_64_instr_token::number_literal_tt);
                }
            }

            std::string normal_number;

            while (numeric_set.contains(source[pos]) || source[pos] == '-' || source[pos] == '+') {
                normal_number += source[pos];
                pos += 1;
            }

            return assembler_token(normal_number, x86_64_instr_token::number_literal_tt);
        }

        assembler_token handle_symbole() {
            const auto start_pos = pos;

            while (pos < source.length() && alnumeric_set.contains(source[pos])) {
                pos += 1;
            }

            const auto lexeme = source.substr(start_pos, pos - start_pos);

            if (assembler_kw_map.contains(lexeme)) {
                return assembler_token(lexeme, assembler_kw_map[lexeme]);
            }

            return assembler_token("", x86_64_instr_token::unknown_tt);
        }

        assembler_token get_next_token() {
            if (pos >= source.length()) { return assembler_token("end of assembly", x86_64_instr_token::end_of_assembly_tt); }

            handle_whitespace();

            if (pos < source.length() && numeric_set.contains(source[pos])) {
                return handle_numeric();
            }

            if (pos < source.length() && assembler_operator_map.contains(source[pos])) {
                char ch = source[pos];
                pos += 1;  
                return assembler_token(std::string(1, ch), assembler_operator_map[ch]);
            }

            if (pos < source.length() && alnumeric_set.contains(source[pos])) {
                return handle_symbole();
            }

            return assembler_token("", x86_64_instr_token::unknown_tt);
        }

        bool debug;

        void visualize() {
            for (size_t i = 0; i < assembler_token_vec.size(); ++i) {
                auto s = assembler_token_vec.at(i);

                std::cout << "Lexeme: " << s.lexeme << "\n"
                          << "Type: " << assembler_token::get_typename(s.tt) << "\n";
            }
        }
        
        assembler_token peek(std::size_t i = 0) {
            return assembler_token_vec.at(pos_stream + i);
        }

        void consume(std::size_t i = 0) {
            pos_stream += 1 + i;
        }

        std::int64_t parse_signed_imm() {
            bool negative = false;

            if (peek().tt == x86_64_instr_token::minus_tt) {
                negative = true;
                consume();
            }

            if (peek().tt != x86_64_instr_token::number_literal_tt) {
                throw std::runtime_error("expected numeric literal in assembler");
            }

            auto tok = peek();
            consume();

            std::int64_t val = from_numerical_string<std::int64_t>(tok.lexeme);
            return negative ? -val : val;
        }

        // value (0, 1, 2, 3) expected by the mem struct.
        static std::size_t encode_scale(std::size_t actual) {
            switch (actual) {
                case 2: return 1;
                case 4: return 2;
                case 8: return 3;
                default: return 0; 
            }
        }

        // [reg], [reg+disp], [reg-disp], [reg+idx], [reg+idx*scale], [reg+idx*scale+disp], [reg+idx*scale-disp]
        mem parse_mem() {
            if (!is_gpr(peek().tt)) {
                throw std::runtime_error("expected base register in memory operand");
            }

            auto base_tok = peek();
            consume();
            grp base_reg = assembler_token_to_gpr(base_tok.tt);

            if (peek().tt == x86_64_instr_token::bracket_right_tt) {
                consume();
                return mem{base_reg};
            }

            if (peek().tt == x86_64_instr_token::minus_tt) {
                consume(); 

                if (peek().tt != x86_64_instr_token::number_literal_tt) {
                    throw std::runtime_error("expected displacement after '-' in memory operand");
                }

                auto disp_tok = peek();
                consume();
                std::int32_t disp = -from_numerical_string<std::int32_t>(disp_tok.lexeme);

                if (peek().tt != x86_64_instr_token::bracket_right_tt) {
                    throw std::runtime_error("expected ']' after displacement in memory operand");
                }
                consume(); 
                return mem{base_reg, disp};
            }

            if (peek().tt == x86_64_instr_token::addition_tt) {
                consume(); // consume '+'

                if (peek().tt == x86_64_instr_token::number_literal_tt) {
                    // [reg + disp]
                    auto disp_tok = peek();
                    consume();
                    std::int32_t disp = from_numerical_string<std::int32_t>(disp_tok.lexeme);

                    if (peek().tt != x86_64_instr_token::bracket_right_tt) {
                        throw std::runtime_error("expected ']' after displacement in memory operand");
                    }
                    consume();
                    return mem{base_reg, disp};
                }

                if (is_gpr(peek().tt)) {
                    auto idx_tok = peek();
                    consume();
                    grp idx_reg = assembler_token_to_gpr(idx_tok.tt);

                    if (peek().tt == x86_64_instr_token::bracket_right_tt) {
                        // [reg + idx] — scale factor 1, SIB-encoded as 0
                        consume(); 
                        return mem{base_reg, idx_reg, static_cast<std::size_t>(0)};
                    }

                    if (peek().tt == x86_64_instr_token::multiplication_tt) {
                        consume(); 

                        if (peek().tt != x86_64_instr_token::number_literal_tt) {
                            throw std::runtime_error("expected scale after '*' in memory operand");
                        }

                        auto scale_tok = peek();
                        consume();
                        std::size_t scale = encode_scale(from_numerical_string<std::size_t>(scale_tok.lexeme));

                        if (peek().tt == x86_64_instr_token::bracket_right_tt) {
                            // [reg + idx*scale]
                            consume();
                            return mem{base_reg, idx_reg, scale};
                        }

                        if (peek().tt == x86_64_instr_token::addition_tt || peek().tt == x86_64_instr_token::minus_tt) {
                            bool neg = peek().tt == x86_64_instr_token::minus_tt;
                            consume(); 

                            if (peek().tt != x86_64_instr_token::number_literal_tt) {
                                throw std::runtime_error("expected displacement after scale in memory operand");
                            }

                            auto disp_tok = peek();
                            consume();
                            std::int32_t disp = from_numerical_string<std::int32_t>(disp_tok.lexeme);
                            if (neg) disp = -disp;

                            if (peek().tt != x86_64_instr_token::bracket_right_tt) {
                                throw std::runtime_error("expected ']' in memory operand");
                            }
                            consume(); 
                            return mem{base_reg, idx_reg, scale, disp};
                        }
                    }
                }
            }

            throw std::runtime_error("invalid memory addressing mode in assembler");
        }

#define EMIT_ARITH(X) \
    consume(); \
    if (peek().tt == x86_64_instr_token::bracket_left_tt) { \
        consume(); \
        mem dest_mem = parse_mem(); \
        if (peek().tt != x86_64_instr_token::comma_tt) { \
            throw std::runtime_error("expected comma in assembler"); \
        } \
        consume(); \
        if (peek().tt == x86_64_instr_token::number_literal_tt || \
            peek().tt == x86_64_instr_token::minus_tt) { \
            std::int32_t imm = static_cast<std::int32_t>(parse_signed_imm()); \
            w.emit_##X(dest_mem, imm); \
        } \
        else if (is_gpr(peek().tt)) { \
            auto reg2 = peek(); \
            consume(); \
            w.emit_##X(dest_mem, assembler_token_to_gpr(reg2.tt)); \
        } \
        else { \
            throw std::runtime_error("unsupported source operand for " #X " [mem], X"); \
        } \
    } \
    else if (is_gpr(peek().tt)) { \
        auto reg = peek(); \
        consume(); \
        if (peek().tt != x86_64_instr_token::comma_tt) { \
            throw std::runtime_error("expected comma in assembler"); \
        } \
        consume(); \
        if (peek().tt == x86_64_instr_token::number_literal_tt || \
            peek().tt == x86_64_instr_token::minus_tt) { \
            std::int32_t imm = static_cast<std::int32_t>(parse_signed_imm()); \
            w.emit_##X(assembler_token_to_gpr(reg.tt), imm); \
        } \
        else if (peek().tt == x86_64_instr_token::bracket_left_tt) { \
            consume(); \
            mem src_mem = parse_mem(); \
            w.emit_##X(assembler_token_to_gpr(reg.tt), src_mem); \
        } \
        else if (is_gpr(peek().tt)) { \
            auto reg2 = peek(); \
            consume(); \
            w.emit_##X(assembler_token_to_gpr(reg.tt), assembler_token_to_gpr(reg2.tt)); \
        } \
        else { \
            throw std::runtime_error("unsupported source operand for " #X " reg, X"); \
        } \
    } \
    else { \
        throw std::runtime_error("unsupported destination operand for " #X); \
    }
    

    public:
        void assemble() {
            assembler_token token = get_next_token();

            while (token.tt != x86_64_instr_token::end_of_assembly_tt) {
                assembler_token_vec.push_back(token);
                token = get_next_token();
            }

            assembler_token_vec.push_back(token);

            /*if (debug) {
                visualize();
            }*/

            while (pos_stream < assembler_token_vec.size()) {
                auto& instr = assembler_token_vec.at(pos_stream);

                switch(instr.tt) {
                    case x86_64_instr_token::xor_tt: { EMIT_ARITH(xor) break; }
                    case x86_64_instr_token::sub_tt: { EMIT_ARITH(sub) break; }
                    case x86_64_instr_token::add_tt: { EMIT_ARITH(add) break; }
                    case x86_64_instr_token::mov_tt: {
                        consume(); 

                        if (peek().tt == x86_64_instr_token::bracket_left_tt) { // destination is memory
                            consume(); 

                            mem dest_mem = parse_mem();

                            if (peek().tt != x86_64_instr_token::comma_tt) {
                                throw std::runtime_error("expected comma in assembler");
                            }
                            consume(); 

                            if (peek().tt == x86_64_instr_token::number_literal_tt ||
                                peek().tt == x86_64_instr_token::minus_tt) { // mov [mem], imm
                                std::int32_t imm = static_cast<std::int32_t>(parse_signed_imm());
                                w.emit_mov(dest_mem, imm);
                            }
                            else if (is_gpr(peek().tt)) { // mov [mem], reg
                                auto reg2 = peek();
                                consume();
                                w.emit_mov(dest_mem, assembler_token_to_gpr(reg2.tt));
                            }
                            else {
                                throw std::runtime_error("unsupported source operand for mov [mem], X");
                            }
                        }
                        else if (is_gpr(peek().tt)) { // destination is register
                            auto reg = peek();
                            consume();

                            if (peek().tt != x86_64_instr_token::comma_tt) {
                                throw std::runtime_error("expected comma in assembler");
                            }
                            consume(); 

                            if (peek().tt == x86_64_instr_token::number_literal_tt ||
                                peek().tt == x86_64_instr_token::minus_tt) { // mov reg, imm
                                std::int64_t imm = parse_signed_imm();
                                w.emit_mov(assembler_token_to_gpr(reg.tt), imm);
                            }
                            else if (peek().tt == x86_64_instr_token::bracket_left_tt) { // mov reg, [mem]
                                consume(); 
                                mem src_mem = parse_mem();
                                w.emit_mov(assembler_token_to_gpr(reg.tt), src_mem);
                            }
                            else if (is_gpr(peek().tt)) { // mov reg, reg
                                auto reg2 = peek();
                                consume();
                                w.emit_mov(assembler_token_to_gpr(reg.tt), assembler_token_to_gpr(reg2.tt));
                            }
                            else {
                                throw std::runtime_error("unsupported source operand for mov reg, X");
                            }
                        }
                        else {
                            throw std::runtime_error("unsupported destination operand for mov");
                        }

                        break;
                    }
                    default: {
                        pos_stream++;
                        break;
                    }
                }
            }

            if (debug) {
                std::cout << "Assembled bytes: \n";
                w.print_bytes();
            }
        }
        
        assembler(const std::string& assembly, const bool debug = false) : source(assembly), debug(debug) {}
    };
} // namespace occult::x86_64