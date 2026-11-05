#pragma once
#include <cstdint>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace occult::x86_64 {
    // what a `{name}` operand turns into
    struct asm_variable {
        std::int32_t rbp_offset; // address is [rbp + rbp_offset]
        int size_bits;           // 8/16/32/64, 0 when the type has no scalar size
    };

    struct asm_symbol_use {
        std::string name;
        std::size_t at;  // call: offset of the E8 opcode; address: offset of the rip disp32
        bool is_jump;    // jmp to a function (E9) instead of call (E8)
        // [rip + name + n]: the n. whoever patches the symbol in adds it back.
        std::int64_t addend = 0;
        std::size_t end = 0;
    };

    struct asm_output {
        std::vector<std::uint8_t> bytes;
        std::vector<asm_symbol_use> calls;        // call/jmp rel32 to an Occult function
        std::vector<asm_symbol_use> addresses;    // [rip + function] operands
        std::vector<asm_symbol_use> globals;      // @global operands (offset of the imm64 slot)
    };

    class assembler {
    public:
        using variable_lookup = std::function<std::optional<asm_variable>(const std::string&)>;
        using global_lookup = std::function<bool(const std::string&)>;
        using function_lookup = std::function<bool(const std::string&)>;

        explicit assembler(std::string source, variable_lookup vars = {}, function_lookup funcs = {}, global_lookup globals = {})
            : src(std::move(source)), find_var(std::move(vars)), is_function(std::move(funcs)), is_global(std::move(globals)) {}

        asm_output assemble() {
            tokenize();
            for (line_no = 0; line_no < lines.size(); ++line_no) {
                try {
                    assemble_line(lines[line_no]);
                }
                catch (const std::runtime_error& e) {
                    throw std::runtime_error("asm line " + std::to_string(lines[line_no].line) + ": " + e.what());
                }
            }
            resolve_fixups();
            return std::move(out);
        }

    private:
        // ---------------------------------------------------------------- lexing
        enum class tk { id, num, str, punct, var };

        struct token {
            tk kind;
            std::string text;
            std::int64_t num = 0;
        };

        struct line_t {
            std::size_t line;
            std::vector<token> toks;
        };

        std::string src;
        variable_lookup find_var;
        function_lookup is_function;
        global_lookup is_global;
        std::vector<line_t> lines;
        std::size_t line_no = 0;
        asm_output out;

        static bool id_start(char c) { return std::isalpha(static_cast<unsigned char>(c)) || c == '_' || c == '.'; }
        static bool id_char(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '.' || c == '$'; }

        void tokenize() {
            std::size_t i = 0, ln = 1;
            line_t cur{1, {}};
            auto flush = [&] {
                if (!cur.toks.empty())
                    lines.push_back(std::move(cur));
                cur = line_t{ln, {}};
            };
            while (i < src.size()) {
                const char c = src[i];
                if (c == '\n') {
                    ++i;
                    ++ln;
                    flush();
                    cur.line = ln;
                    continue;
                }
                if (c == ' ' || c == '\t' || c == '\r') {
                    ++i;
                    continue;
                }
                if (c == ';' || (c == '/' && i + 1 < src.size() && src[i + 1] == '/')) {
                    while (i < src.size() && src[i] != '\n')
                        ++i;
                    continue;
                }
                if (c == '/' && i + 1 < src.size() && src[i + 1] == '*') {
                    i += 2;
                    while (i + 1 < src.size() && !(src[i] == '*' && src[i + 1] == '/')) {
                        if (src[i] == '\n')
                            ++ln;
                        ++i;
                    }
                    i += 2;
                    continue;
                }
                if (std::isdigit(static_cast<unsigned char>(c))) {
                    cur.toks.push_back(lex_number(i, ln));
                    continue;
                }
                if (id_start(c)) {
                    std::size_t s = i;
                    while (i < src.size() && id_char(src[i]))
                        ++i;
                    cur.toks.push_back({tk::id, src.substr(s, i - s)});
                    continue;
                }
                if (c == '"') {
                    cur.toks.push_back({tk::str, lex_string(i, ln, '"')});
                    continue;
                }
                if (c == '\'') {
                    const std::string s = lex_string(i, ln, '\'');
                    if (s.size() != 1)
                        throw std::runtime_error("asm line " + std::to_string(ln) + ": character literal must be one character");
                    token t{tk::num, s};
                    t.num = static_cast<unsigned char>(s[0]);
                    cur.toks.push_back(t);
                    continue;
                }
                if (c == '{') { // {variable}
                    std::size_t j = i + 1;
                    while (j < src.size() && (src[j] == ' ' || src[j] == '\t'))
                        ++j;
                    std::size_t s = j;
                    while (j < src.size() && id_char(src[j]))
                        ++j;
                    const std::string name = src.substr(s, j - s);
                    while (j < src.size() && (src[j] == ' ' || src[j] == '\t'))
                        ++j;
                    if (name.empty() || j >= src.size() || src[j] != '}')
                        throw std::runtime_error("asm line " + std::to_string(ln) + ": expected {variable}");
                    cur.toks.push_back({tk::var, name});
                    i = j + 1;
                    continue;
                }
                if ((c == '<' || c == '>') && i + 1 < src.size() && src[i + 1] == c) {
                    cur.toks.push_back({tk::punct, std::string(2, c)});
                    i += 2;
                    continue;
                }
                if (std::strchr("[]+-*/%,:()&|^~@", c)) {
                    cur.toks.push_back({tk::punct, std::string(1, c)});
                    ++i;
                    continue;
                }
                throw std::runtime_error("asm line " + std::to_string(ln) + ": unexpected character '" + std::string(1, c) + "'");
            }
            flush();
        }

        token lex_number(std::size_t& i, std::size_t ln) {
            int base = 10;
            std::size_t s = i;
            if (src[i] == '0' && i + 1 < src.size()) {
                const char n = src[i + 1];
                if (n == 'x' || n == 'X') { base = 16; i += 2; s = i; }
                else if (n == 'b' || n == 'B') { base = 2; i += 2; s = i; }
                else if (n == 'o' || n == 'O') { base = 8; i += 2; s = i; }
            }
            std::uint64_t v = 0;
            std::size_t digits = 0;
            while (i < src.size() && std::isxdigit(static_cast<unsigned char>(src[i])) && (base == 16 || std::isdigit(static_cast<unsigned char>(src[i])))) {
                const int d = std::isdigit(static_cast<unsigned char>(src[i])) ? src[i] - '0' : (std::tolower(src[i]) - 'a' + 10);
                if (d >= base)
                    throw std::runtime_error("asm line " + std::to_string(ln) + ": bad digit in number");
                v = v * static_cast<std::uint64_t>(base) + static_cast<std::uint64_t>(d);
                ++i;
                ++digits;
            }
            if (digits == 0)
                throw std::runtime_error("asm line " + std::to_string(ln) + ": malformed number");
            token t{tk::num, src.substr(s, i - s)};
            t.num = static_cast<std::int64_t>(v);
            return t;
        }

        std::string lex_string(std::size_t& i, std::size_t ln, char quote) {
            std::string s;
            ++i;
            while (i < src.size() && src[i] != quote) {
                char c = src[i++];
                if (c == '\n')
                    throw std::runtime_error("asm line " + std::to_string(ln) + ": unterminated string");
                if (c == '\\' && i < src.size()) {
                    c = src[i++];
                    switch (c) {
                    case 'n': s += '\n'; break;
                    case 't': s += '\t'; break;
                    case 'r': s += '\r'; break;
                    case '0': s += '\0'; break;
                    case '\\': s += '\\'; break;
                    case '"': s += '"'; break;
                    case '\'': s += '\''; break;
                    case 'x': {
                        int v = 0, n = 0;
                        while (n < 2 && i < src.size() && std::isxdigit(static_cast<unsigned char>(src[i]))) {
                            v = v * 16 + (std::isdigit(static_cast<unsigned char>(src[i])) ? src[i] - '0' : std::tolower(src[i]) - 'a' + 10);
                            ++i;
                            ++n;
                        }
                        s += static_cast<char>(v);
                        break;
                    }
                    default: s += c;
                    }
                }
                else {
                    s += c;
                }
            }
            if (i >= src.size())
                throw std::runtime_error("asm line " + std::to_string(ln) + ": unterminated string");
            ++i;
            return s;
        }

        // ---------------------------------------------------------------- registers
        struct reg_info {
            int idx;
            int size; // 8,16,32,64,128 (xmm)
            bool high8 = false;
        };

        static const std::unordered_map<std::string, reg_info>& registers() {
            static const std::unordered_map<std::string, reg_info> m = [] {
                std::unordered_map<std::string, reg_info> r;
                const char* n64[] = {"rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi"};
                const char* n32[] = {"eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi"};
                const char* n16[] = {"ax", "cx", "dx", "bx", "sp", "bp", "si", "di"};
                const char* n8[] = {"al", "cl", "dl", "bl", "spl", "bpl", "sil", "dil"};
                for (int i = 0; i < 8; ++i) {
                    r[n64[i]] = {i, 64};
                    r[n32[i]] = {i, 32};
                    r[n16[i]] = {i, 16};
                    r[n8[i]] = {i, 8};
                }
                r["ah"] = {4, 8, true};
                r["ch"] = {5, 8, true};
                r["dh"] = {6, 8, true};
                r["bh"] = {7, 8, true};
                for (int i = 8; i < 16; ++i) {
                    const std::string b = "r" + std::to_string(i);
                    r[b] = {i, 64};
                    r[b + "d"] = {i, 32};
                    r[b + "w"] = {i, 16};
                    r[b + "b"] = {i, 8};
                    r[b + "l"] = {i, 8};
                }
                for (int i = 0; i < 16; ++i)
                    r["xmm" + std::to_string(i)] = {i, 128};
                return r;
            }();
            return m;
        }

        static std::string lower(std::string s) {
            for (auto& c : s)
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return s;
        }

        // ---------------------------------------------------------------- operands
        enum class ot { none, reg, mem, imm, sym };

        struct operand {
            ot t = ot::none;
            int size = 0; // bits; 0 = not given
            int reg = 0;
            bool xmm = false;
            bool high8 = false;
            bool rex8 = false; // spl/bpl/sil/dil need a REX prefix
            // memory
            int base = -1, index = -1;
            int scale = 1;
            std::int64_t disp = 0;
            bool rip = false;
            std::string label; // rip + label, or a branch target (sym)
            int seg = 0;       // 0x64 fs, 0x65 gs
            std::string var;   // set when the operand came from {name}
            // immediate
            std::int64_t imm = 0;
            std::vector<token> expr; // non-empty: the value involves labels (e.g. end - start) and is filled in later
            std::string imm_symbol; // set by @global: the address is filled in by the compiler
        };

        struct cursor {
            const std::vector<token>* toks;
            std::size_t i = 0;
            bool end() const { return i >= toks->size(); }
            const token& cur() const { return (*toks)[i]; }
            bool is_punct(const char* p) const { return !end() && cur().kind == tk::punct && cur().text == p; }
            bool is_id(const char* p) const { return !end() && cur().kind == tk::id && lower(cur().text) == p; }
        };

        // constant expression; labels contribute through `lab` so differences work
        struct value {
            std::int64_t v = 0;
            int lab = 0; // net number of label references (+1 for `a`, 0 for `a - b`)
        };

        using label_value = std::function<std::optional<std::int64_t>(const std::string&)>;

        value parse_primary(cursor& c, const label_value& labels) {
            if (c.end())
                throw std::runtime_error("expected a value");
            const token t = c.cur();
            if (t.kind == tk::num) {
                ++c.i;
                return {t.num, 0};
            }
            if (t.kind == tk::punct && t.text == "(") {
                ++c.i;
                value v = parse_expr(c, labels, 0);
                if (!c.is_punct(")"))
                    throw std::runtime_error("expected ')'");
                ++c.i;
                return v;
            }
            if (t.kind == tk::punct && (t.text == "-" || t.text == "+" || t.text == "~")) {
                ++c.i;
                value v = parse_primary(c, labels);
                if (t.text == "-") { v.v = -v.v; v.lab = -v.lab; }
                else if (t.text == "~") { v.v = ~v.v; if (v.lab) throw std::runtime_error("cannot apply ~ to a label"); }
                return v;
            }
            if (t.kind == tk::id) {
                if (!labels)
                    throw std::runtime_error("'" + t.text + "' is not a constant here");
                auto lv = labels(t.text);
                if (!lv)
                    throw std::runtime_error("unknown symbol '" + t.text + "'");
                ++c.i;
                return {*lv, 1};
            }
            throw std::runtime_error("expected a value, got '" + t.text + "'");
        }

        static int prec_of(const std::string& op) {
            if (op == "|") return 1;
            if (op == "^") return 2;
            if (op == "&") return 3;
            if (op == "<<" || op == ">>") return 4;
            if (op == "+" || op == "-") return 5;
            if (op == "*" || op == "/" || op == "%") return 6;
            return -1;
        }

        value parse_expr(cursor& c, const label_value& labels, int min_prec) {
            value lhs = parse_primary(c, labels);
            while (!c.end() && c.cur().kind == tk::punct) {
                const std::string op = c.cur().text;
                const int p = prec_of(op);
                if (p < 0 || p < min_prec || p == 0)
                    break;
                ++c.i;
                value rhs = parse_expr(c, labels, p + 1);
                if (op == "+") { lhs.v += rhs.v; lhs.lab += rhs.lab; }
                else if (op == "-") { lhs.v -= rhs.v; lhs.lab -= rhs.lab; }
                else {
                    if (lhs.lab || rhs.lab)
                        throw std::runtime_error("labels can only be added or subtracted");
                    if (op == "*") lhs.v *= rhs.v;
                    else if (op == "/") { if (!rhs.v) throw std::runtime_error("division by zero"); lhs.v /= rhs.v; }
                    else if (op == "%") { if (!rhs.v) throw std::runtime_error("division by zero"); lhs.v %= rhs.v; }
                    else if (op == "<<") lhs.v <<= rhs.v;
                    else if (op == ">>") lhs.v >>= rhs.v;
                    else if (op == "&") lhs.v &= rhs.v;
                    else if (op == "|") lhs.v |= rhs.v;
                    else lhs.v ^= rhs.v;
                }
            }
            return lhs;
        }

        std::int64_t parse_const(cursor& c) {
            const value v = parse_expr(c, nullptr, 0);
            return v.v;
        }

        static int size_keyword(const std::string& w) {
            const std::string s = lower(w);
            if (s == "byte") return 8;
            if (s == "word") return 16;
            if (s == "dword") return 32;
            if (s == "qword") return 64;
            if (s == "xmmword" || s == "oword") return 128;
            return 0;
        }

        operand var_operand(const std::string& name) {
            if (!find_var)
                throw std::runtime_error("variables like {" + name + "} are not available in this asm block");
            auto v = find_var(name);
            if (!v)
                throw std::runtime_error("unknown variable {" + name + "}");
            operand o;
            o.t = ot::mem;
            o.base = 5; // rbp
            o.disp = v->rbp_offset;
            o.size = v->size_bits;
            o.var = name;
            return o;
        }

        operand parse_memory(cursor& c, int size) {
            // current token is '['
            ++c.i;
            operand o;
            o.t = ot::mem;
            o.size = size;
            bool negative = false;
            bool first = true;
            while (true) {
                if (c.end())
                    throw std::runtime_error("expected ']'");
                if (c.is_punct("]")) {
                    ++c.i;
                    break;
                }
                if (!first) {
                    if (c.is_punct("+")) { negative = false; ++c.i; }
                    else if (c.is_punct("-")) { negative = true; ++c.i; }
                    else throw std::runtime_error("expected '+', '-' or ']' in memory operand");
                }
                else {
                    negative = false;
                    if (c.is_punct("-")) { negative = true; ++c.i; }
                    else if (c.is_punct("+")) ++c.i;
                }
                first = false;

                // one term: reg | reg*n | n*reg | const-expr | label (after rip)
                if (!c.end() && c.cur().kind == tk::id) {
                    const std::string w = lower(c.cur().text);
                    if (w == "rip") {
                        if (negative) throw std::runtime_error("cannot subtract rip");
                        ++c.i;
                        o.rip = true;
                        continue;
                    }
                    auto r = registers().find(w);
                    if (r == registers().end()) {
                        if (negative) throw std::runtime_error("cannot subtract a symbol");
                        o.label = c.cur().text;
                        ++c.i;
                        continue;
                    }
                    if (r->second.size != 64)
                        throw std::runtime_error("memory operands use 64-bit registers (" + w + ")");
                    ++c.i;
                    int scale = 1;
                    if (c.is_punct("*")) {
                        ++c.i;
                        scale = static_cast<int>(parse_primary_const(c));
                        if (scale != 1 && scale != 2 && scale != 4 && scale != 8)
                            throw std::runtime_error("scale must be 1, 2, 4 or 8");
                    }
                    if (negative)
                        throw std::runtime_error("cannot subtract a register");
                    if (scale == 1 && o.base < 0 && o.index < 0) o.base = r->second.idx;
                    else if (o.index < 0 && (scale != 1 || o.base >= 0)) { o.index = r->second.idx; o.scale = scale; }
                    else if (o.base < 0) o.base = r->second.idx;
                    else throw std::runtime_error("too many registers in memory operand");
                    continue;
                }
                // number first: maybe `n*reg`
                std::int64_t n = parse_primary_const(c);
                if (c.is_punct("*") && c.i + 1 < c.toks->size() && (*c.toks)[c.i + 1].kind == tk::id && registers().count(lower((*c.toks)[c.i + 1].text))) {
                    ++c.i;
                    const auto r = registers().at(lower(c.cur().text));
                    ++c.i;
                    if (n != 1 && n != 2 && n != 4 && n != 8)
                        throw std::runtime_error("scale must be 1, 2, 4 or 8");
                    if (o.index >= 0) throw std::runtime_error("too many registers in memory operand");
                    o.index = r.idx;
                    o.scale = static_cast<int>(n);
                    continue;
                }
                // rest of a constant product like 8*2
                while (c.is_punct("*")) {
                    ++c.i;
                    n *= parse_primary_const(c);
                }
                o.disp += negative ? -n : n;
            }
            if (o.rip && (o.base >= 0 || o.index >= 0))
                throw std::runtime_error("rip-relative operands cannot have a base or index");
            if (!o.label.empty() && !o.rip)
                throw std::runtime_error("use [rip + " + o.label + "] to reference a label");
            return o;
        }

        std::int64_t parse_primary_const(cursor& c) {
            const value v = parse_primary(c, nullptr);
            return v.v;
        }

        bool at_operand_end(const cursor& c) const { return c.end() || c.is_punct(","); }

        operand parse_operand(cursor& c) {
            int size = 0;
            // size keyword (+ optional "ptr")
            if (!c.end() && c.cur().kind == tk::id && size_keyword(c.cur().text)) {
                size = size_keyword(c.cur().text);
                ++c.i;
                if (c.is_id("ptr"))
                    ++c.i;
            }
            int seg = 0;
            if (!c.end() && c.cur().kind == tk::id && (c.is_id("fs") || c.is_id("gs")) && c.i + 1 < c.toks->size() && (*c.toks)[c.i + 1].kind == tk::punct && (*c.toks)[c.i + 1].text == ":") {
                seg = c.is_id("fs") ? 0x64 : 0x65;
                c.i += 2;
            }
            if (c.is_punct("[")) {
                operand o = parse_memory(c, size);
                o.seg = seg;
                return o;
            }
            if (seg)
                throw std::runtime_error("segment override needs a memory operand");
            if (c.is_punct("@")) {
                ++c.i;
                if (c.end() || c.cur().kind != tk::id)
                    throw std::runtime_error("@ must be followed by a global's name");
                const std::string name = c.cur().text;
                ++c.i;
                if (!is_global || !is_global(name))
                    throw std::runtime_error("'" + name + "' is not a global variable");
                operand o;
                o.t = ot::imm;
                o.size = 64;
                o.imm_symbol = name;
                return o;
            }
            if (!c.end() && c.cur().kind == tk::var) {
                operand o = var_operand(c.cur().text);
                ++c.i;
                if (size) {
                    if (o.size && o.size != size)
                        throw std::runtime_error("{" + o.var + "} is " + std::to_string(o.size) + "-bit, not " + std::to_string(size));
                    o.size = size;
                }
                return o;
            }
            if (!c.end() && c.cur().kind == tk::id) {
                const std::string w = lower(c.cur().text);
                if (auto r = registers().find(w); r != registers().end()) {
                    if (size)
                        throw std::runtime_error("size keyword before a register");
                    ++c.i;
                    operand o;
                    o.t = ot::reg;
                    o.reg = r->second.idx;
                    o.size = r->second.size;
                    o.xmm = r->second.size == 128;
                    o.high8 = r->second.high8;
                    o.rex8 = r->second.size == 8 && !r->second.high8 && r->second.idx >= 4 && r->second.idx < 8;
                    return o;
                }
                // a lone identifier is a branch target / symbol
                if (c.i + 1 >= c.toks->size() || ((*c.toks)[c.i + 1].kind == tk::punct && (*c.toks)[c.i + 1].text == ",")) {
                    operand o;
                    o.t = ot::sym;
                    o.label = c.cur().text;
                    ++c.i;
                    return o;
                }
            }
            operand o;
            o.t = ot::imm;
            o.size = size;
            const std::size_t start = c.i;
            bool saw_label = false;
            const value v = parse_expr(c, [&](const std::string&) -> std::optional<std::int64_t> { saw_label = true; return 0; }, 0);
            if (saw_label) {
                if (v.lab != 0)
                    throw std::runtime_error("a label's absolute address is not known here; use a difference of two labels or [rip + label]");
                o.expr.assign(c.toks->begin() + static_cast<std::ptrdiff_t>(start), c.toks->begin() + static_cast<std::ptrdiff_t>(c.i));
            }
            else {
                o.imm = v.v;
            }
            return o;
        }

        // ---------------------------------------------------------------- output + fixups
        struct label_def {
            std::size_t pos;
        };
        enum class fix_kind { data, call, jmp, branch, rip };
        struct fixup {
            fix_kind kind;
            std::size_t at;       // where the field is
            int bytes;            // 1, 4, 8
            bool relative;        // rel: value = target - end ; data: value = expression
            std::size_t end;      // relative: address the CPU measures from
            std::string label;    // relative target
            std::vector<token> expr; // data expression
            std::int64_t addend = 0;
            std::size_t line;
        };
        std::map<std::string, std::size_t> labels_;
        std::vector<fixup> fixups_;

        std::size_t here() const { return out.bytes.size(); }
        void b(std::uint8_t v) { out.bytes.push_back(v); }
        void bytes(std::initializer_list<std::uint8_t> v) { for (auto x : v) b(x); }
        void imm_n(std::int64_t v, int n) { for (int i = 0; i < n; ++i) b(static_cast<std::uint8_t>((static_cast<std::uint64_t>(v) >> (8 * i)) & 0xFF)); }

        static bool fits8(std::int64_t v) { return v >= -128 && v <= 127; }
        static bool fits8i(const operand& o) { return o.expr.empty() && fits8(o.imm); }
        static bool fits32i(const operand& o) {
            if (!o.imm_symbol.empty()) return false; // @global always takes the imm64 form
            return !o.expr.empty() || (o.imm >= INT32_MIN && o.imm <= INT32_MAX);
        }

        // An immediate field. Label expressions are evaluated once every label is known.
        void emit_imm(const operand& o, int n) {
            if (!o.imm_symbol.empty()) {
                // @global is patched at codegen and relocated at link time.
                if (n != 8)
                    throw std::runtime_error("@" + o.imm_symbol + " is a 64-bit address; use a 64-bit register");
                out.globals.push_back({o.imm_symbol, here(), false});
                imm_n(0, n);
                return;
            }
            if (o.expr.empty()) {
                imm_n(o.imm, n);
                return;
            }
            fixup f;
            f.kind = fix_kind::data;
            f.at = here();
            f.bytes = n;
            f.relative = false;
            f.end = 0;
            f.expr = o.expr;
            f.line = lines[line_no].line;
            imm_n(0, n);
            fixups_.push_back(std::move(f));
        }
        static bool fits32(std::int64_t v) { return v >= INT32_MIN && v <= INT32_MAX; }

        // prefixes entered on this line (lock/rep)
        std::vector<std::uint8_t> line_prefixes;

        // ModRM/SIB/disp + REX + opcode. regfield may be an extension digit.
        void modrm_op(std::uint8_t mandatory, bool p66, bool rexw, std::initializer_list<std::uint8_t> opcode, int regfield, const operand& rm, int imm_bytes, bool force_rex = false) {
            for (auto p : line_prefixes) b(p);
            if (rm.t == ot::mem && rm.seg) b(static_cast<std::uint8_t>(rm.seg));
            if (p66) b(0x66);
            if (mandatory) b(mandatory);

            int rexr = (regfield >> 3) & 1, rexx = 0, rexb = 0;
            if (rm.t == ot::reg) {
                rexb = (rm.reg >> 3) & 1;
            }
            else {
                if (rm.base >= 0) rexb = (rm.base >> 3) & 1;
                if (rm.index >= 0) rexx = (rm.index >> 3) & 1;
            }
            const std::uint8_t rex = static_cast<std::uint8_t>(0x40 | (rexw ? 8 : 0) | (rexr << 2) | (rexx << 1) | rexb);
            if (rex != 0x40 || force_rex)
                b(rex);
            for (auto o : opcode) b(o);

            const int reg3 = regfield & 7;
            if (rm.t == ot::reg) {
                b(static_cast<std::uint8_t>(0xC0 | (reg3 << 3) | (rm.reg & 7)));
                return;
            }
            // memory
            if (rm.rip) {
                b(static_cast<std::uint8_t>((reg3 << 3) | 5));
                const std::size_t at = here();
                imm_n(rm.disp, 4);
                if (!rm.label.empty()) { // a label in this block, or an Occult function (patched by codegen)
                    fixups_.push_back({fix_kind::rip, at, 4, true, at + 4 + static_cast<std::size_t>(imm_bytes), rm.label, {}, rm.disp, lines[line_no].line});
                }
                return;
            }
            if (rm.base < 0) { // [index*scale + disp] or [disp]
                b(static_cast<std::uint8_t>((reg3 << 3) | 4));
                int ss = rm.scale == 8 ? 3 : rm.scale == 4 ? 2 : rm.scale == 2 ? 1 : 0;
                int idx = rm.index >= 0 ? (rm.index & 7) : 4;
                if (rm.index == 4) throw std::runtime_error("rsp cannot be an index register");
                b(static_cast<std::uint8_t>((ss << 6) | (idx << 3) | 5));
                imm_n(rm.disp, 4);
                return;
            }
            if (rm.index == 4)
                throw std::runtime_error("rsp cannot be an index register");
            const int base3 = rm.base & 7;
            const bool need_sib = rm.index >= 0 || base3 == 4;
            int mod;
            if (rm.disp == 0 && base3 != 5) mod = 0;
            else if (fits8(rm.disp)) mod = 1;
            else mod = 2;
            if (!fits32(rm.disp))
                throw std::runtime_error("displacement does not fit in 32 bits");
            b(static_cast<std::uint8_t>((mod << 6) | (reg3 << 3) | (need_sib ? 4 : base3)));
            if (need_sib) {
                int ss = rm.scale == 8 ? 3 : rm.scale == 4 ? 2 : rm.scale == 2 ? 1 : 0;
                int idx = rm.index >= 0 ? (rm.index & 7) : 4;
                b(static_cast<std::uint8_t>((ss << 6) | (idx << 3) | base3));
            }
            if (mod == 1) imm_n(rm.disp, 1);
            else if (mod == 2) imm_n(rm.disp, 4);
        }

        // opcode with the register in its low bits (push/pop/mov imm/bswap/xchg)
        void reg_op(bool p66, bool rexw, std::initializer_list<std::uint8_t> opcode, int reg, bool force_rex = false) {
            for (auto p : line_prefixes) b(p);
            if (p66) b(0x66);
            const std::uint8_t rex = static_cast<std::uint8_t>(0x40 | (rexw ? 8 : 0) | ((reg >> 3) & 1));
            if (rex != 0x40 || force_rex) b(rex);
            std::size_t n = 0;
            for (auto o : opcode) {
                ++n;
                b(n == opcode.size() ? static_cast<std::uint8_t>(o + (reg & 7)) : o);
            }
        }

        void plain(std::initializer_list<std::uint8_t> opcode, bool p66 = false, bool rexw = false) {
            for (auto p : line_prefixes) b(p);
            if (p66) b(0x66);
            if (rexw) b(0x48);
            for (auto o : opcode) b(o);
        }

        // ---------------------------------------------------------------- helpers for operand sizes
        static int need_size(const operand& a, const operand* other = nullptr) {
            if (a.t == ot::reg) return a.size;
            if (a.size) return a.size;
            if (other && other->t == ot::reg) return other->size;
            return 0;
        }

        static void expect(bool ok, const char* msg) {
            if (!ok) throw std::runtime_error(msg);
        }

        static bool is_gpr(const operand& o) { return o.t == ot::reg && !o.xmm; }
        static bool is_rm(const operand& o) { return o.t == ot::reg || o.t == ot::mem; }

        void check_match(const operand& rm, int size) {
            if (rm.t == ot::mem && rm.size && rm.size != size) {
                throw std::runtime_error(rm.var.empty() ? "operand size mismatch"
                                                         : "{" + rm.var + "} is " + std::to_string(rm.size) + "-bit but the other operand is " + std::to_string(size) + "-bit");
            }
        }

        bool need_rex8(const operand& a, const operand& b2) {
            return (a.t == ot::reg && a.rex8) || (b2.t == ot::reg && b2.rex8);
        }

        // ---------------------------------------------------------------- instruction encoders
        static const std::unordered_map<std::string, int>& cc_table() {
            static const std::unordered_map<std::string, int> m = {
                {"o", 0}, {"no", 1}, {"b", 2}, {"c", 2}, {"nae", 2}, {"ae", 3}, {"nb", 3}, {"nc", 3}, {"e", 4}, {"z", 4}, {"ne", 5}, {"nz", 5}, {"be", 6}, {"na", 6}, {"a", 7}, {"nbe", 7},
                {"s", 8}, {"ns", 9}, {"p", 10}, {"pe", 10}, {"np", 11}, {"po", 11}, {"l", 12}, {"nge", 12}, {"ge", 13}, {"nl", 13}, {"le", 14}, {"ng", 14}, {"g", 15}, {"nle", 15}};
            return m;
        }

        static const std::unordered_map<std::string, int>& alu_table() {
            static const std::unordered_map<std::string, int> m = {{"add", 0}, {"or", 1}, {"adc", 2}, {"sbb", 3}, {"and", 4}, {"sub", 5}, {"xor", 6}, {"cmp", 7}};
            return m;
        }

        static const std::unordered_map<std::string, int>& shift_table() {
            static const std::unordered_map<std::string, int> m = {{"rol", 0}, {"ror", 1}, {"rcl", 2}, {"rcr", 3}, {"shl", 4}, {"sal", 4}, {"shr", 5}, {"sar", 7}};
            return m;
        }

        struct sse_op {
            std::uint8_t prefix; // mandatory prefix or 0
            std::uint8_t opcode;
        };

        static const std::unordered_map<std::string, sse_op>& sse_table() {
            static const std::unordered_map<std::string, sse_op> m = [] {
                std::unordered_map<std::string, sse_op> t;
                const std::pair<const char*, std::uint8_t> arith[] = {{"add", 0x58}, {"mul", 0x59}, {"sub", 0x5C}, {"min", 0x5D}, {"div", 0x5E}, {"max", 0x5F}, {"sqrt", 0x51}};
                for (auto [n, op] : arith) {
                    t[std::string(n) + "ps"] = {0, op};
                    t[std::string(n) + "pd"] = {0x66, op};
                    t[std::string(n) + "ss"] = {0xF3, op};
                    t[std::string(n) + "sd"] = {0xF2, op};
                }
                const std::pair<const char*, std::uint8_t> logic[] = {{"and", 0x54}, {"andn", 0x55}, {"or", 0x56}, {"xor", 0x57}};
                for (auto [n, op] : logic) {
                    t[std::string(n) + "ps"] = {0, op};
                    t[std::string(n) + "pd"] = {0x66, op};
                }
                t["ucomiss"] = {0, 0x2E};
                t["ucomisd"] = {0x66, 0x2E};
                t["comiss"] = {0, 0x2F};
                t["comisd"] = {0x66, 0x2F};
                t["cvtss2sd"] = {0xF3, 0x5A};
                t["cvtsd2ss"] = {0xF2, 0x5A};
                t["cvtps2pd"] = {0, 0x5A};
                t["cvtpd2ps"] = {0x66, 0x5A};
                t["cvtdq2ps"] = {0, 0x5B};
                t["cvtdq2pd"] = {0xF3, 0xE6};
                t["unpcklps"] = {0, 0x14};
                t["unpcklpd"] = {0x66, 0x14};
                t["unpckhps"] = {0, 0x15};
                t["pxor"] = {0x66, 0xEF};
                t["por"] = {0x66, 0xEB};
                t["pand"] = {0x66, 0xDB};
                t["pandn"] = {0x66, 0xDF};
                t["paddb"] = {0x66, 0xFC};
                t["paddw"] = {0x66, 0xFD};
                t["paddd"] = {0x66, 0xFE};
                t["paddq"] = {0x66, 0xD4};
                t["psubb"] = {0x66, 0xF8};
                t["psubw"] = {0x66, 0xF9};
                t["psubd"] = {0x66, 0xFA};
                t["psubq"] = {0x66, 0xFB};
                t["pcmpeqb"] = {0x66, 0x74};
                t["pcmpeqw"] = {0x66, 0x75};
                t["pcmpeqd"] = {0x66, 0x76};
                t["pcmpgtb"] = {0x66, 0x64};
                t["pcmpgtw"] = {0x66, 0x65};
                t["pcmpgtd"] = {0x66, 0x66};
                t["punpcklbw"] = {0x66, 0x60};
                t["punpcklwd"] = {0x66, 0x61};
                t["punpckldq"] = {0x66, 0x62};
                t["punpcklqdq"] = {0x66, 0x6C};
                return t;
            }();
            return m;
        }

        void encode(const std::string& mn, std::vector<operand>& o) {
            const std::size_t n = o.size();

            // ---- no operands
            static const std::unordered_map<std::string, std::vector<std::uint8_t>> simple = {
                {"nop", {0x90}}, {"ret", {0xC3}}, {"retn", {0xC3}}, {"leave", {0xC9}}, {"hlt", {0xF4}}, {"int3", {0xCC}}, {"syscall", {0x0F, 0x05}}, {"sysenter", {0x0F, 0x34}},
                {"cpuid", {0x0F, 0xA2}}, {"rdtsc", {0x0F, 0x31}}, {"rdtscp", {0x0F, 0x01, 0xF9}}, {"ud2", {0x0F, 0x0B}}, {"pause", {0xF3, 0x90}}, {"cld", {0xFC}}, {"std", {0xFD}},
                {"cli", {0xFA}}, {"sti", {0xFB}}, {"clc", {0xF8}}, {"stc", {0xF9}}, {"cmc", {0xF5}}, {"lahf", {0x9F}}, {"sahf", {0x9E}}, {"pushf", {0x9C}}, {"pushfq", {0x9C}},
                {"popf", {0x9D}}, {"popfq", {0x9D}}, {"mfence", {0x0F, 0xAE, 0xF0}}, {"lfence", {0x0F, 0xAE, 0xE8}}, {"sfence", {0x0F, 0xAE, 0xF8}}, {"cdq", {0x99}},
                {"cwde", {0x98}}, {"cqo", {0x48, 0x99}}, {"cdqe", {0x48, 0x98}}, {"cltq", {0x48, 0x98}}, {"cqto", {0x48, 0x99}}, {"cltd", {0x99}}, {"cwtl", {0x98}},
                {"cwd", {0x66, 0x99}}, {"cbw", {0x66, 0x98}}, {"xlatb", {0xD7}}, {"emms", {0x0F, 0x77}}, {"fwait", {0x9B}}, {"wait", {0x9B}}, {"iretq", {0x48, 0xCF}},
                {"movsb", {0xA4}}, {"movsw", {0x66, 0xA5}}, {"movsq", {0x48, 0xA5}}, {"stosb", {0xAA}}, {"stosw", {0x66, 0xAB}}, {"stosd", {0xAB}}, {"stosq", {0x48, 0xAB}},
                {"lodsb", {0xAC}}, {"lodsw", {0x66, 0xAD}}, {"lodsd", {0xAD}}, {"lodsq", {0x48, 0xAD}}, {"scasb", {0xAE}}, {"scasw", {0x66, 0xAF}}, {"scasd", {0xAF}},
                {"scasq", {0x48, 0xAF}}, {"cmpsb", {0xA6}}, {"cmpsw", {0x66, 0xA7}}, {"cmpsd", {0xA7}}, {"cmpsq", {0x48, 0xA7}}, {"movsl", {0xA5}}, {"movsd", {0xA5}}};
            if (n == 0) {
                if (auto s = simple.find(mn); s != simple.end()) {
                    std::size_t first = 0;
                    if (s->second[0] == 0x66) { // operand-size prefix goes before rep/lock
                        b(0x66);
                        first = 1;
                    }
                    for (auto p : line_prefixes) b(p);
                    for (std::size_t k = first; k < s->second.size(); ++k) b(s->second[k]);
                    return;
                }
            }

            // ---- mov family
            if (mn == "mov" || mn == "movabs") return enc_mov(o);
            if (mn == "lea") {
                expect(n == 2 && is_gpr(o[0]) && o[1].t == ot::mem, "lea needs a register and a memory operand");
                expect(o[0].size >= 16, "lea needs a 16/32/64-bit register");
                return modrm_op(0, o[0].size == 16, o[0].size == 64, {0x8D}, o[0].reg, o[1], 0);
            }
            if (alu_table().count(mn)) return enc_alu(alu_table().at(mn), o);
            if (mn == "test") return enc_test(o);
            if (mn == "push") return enc_push(o);
            if (mn == "pop") return enc_pop(o);
            if (mn == "inc" || mn == "dec" || mn == "neg" || mn == "not" || mn == "mul" || mn == "div" || mn == "idiv") return enc_unary(mn, o);
            if (mn == "imul") return enc_imul(o);
            if (shift_table().count(mn)) return enc_shift(shift_table().at(mn), o);
            if (mn == "movzx" || mn == "movsx") return enc_movx(mn == "movsx", o);
            if (mn == "movsxd") {
                expect(n == 2 && is_gpr(o[0]) && is_rm(o[1]), "movsxd needs a register and a 32-bit source");
                if (o[1].t == ot::reg) expect(o[1].size == 32, "movsxd source must be 32-bit");
                else expect(o[1].size == 0 || o[1].size == 32, "movsxd source must be 32-bit");
                return modrm_op(0, false, o[0].size == 64, {0x63}, o[0].reg, o[1], 0);
            }
            if (mn == "xchg") return enc_xchg(o);
            if (mn == "ret" || mn == "retn") {
                expect(n == 1 && o[0].t == ot::imm, "ret takes an optional 16-bit immediate");
                plain({0xC2});
                return imm_n(o[0].imm, 2);
            }
            if (mn == "int") {
                expect(n == 1 && o[0].t == ot::imm, "int needs an immediate");
                if (o[0].imm == 3) return plain({0xCC});
                plain({0xCD});
                return imm_n(o[0].imm, 1);
            }
            if (mn == "nop" && n == 1) {
                expect(is_rm(o[0]), "nop operand must be r/m");
                const int sz = need_size(o[0]);
                return modrm_op(0, sz == 16, false, {0x0F, 0x1F}, 0, o[0], 0);
            }
            if (mn == "call" || mn == "jmp") return enc_call_jmp(mn == "call", o);
            if (mn.size() > 1 && mn[0] == 'j' && cc_table().count(mn.substr(1))) {
                expect(n == 1 && o[0].t == ot::sym, "conditional jump needs a label");
                plain({0x0F, static_cast<std::uint8_t>(0x80 + cc_table().at(mn.substr(1)))});
                return rel_label(o[0].label, 4);
            }
            if (mn == "jrcxz" || mn == "jecxz" || mn == "loop" || mn == "loope" || mn == "loopz" || mn == "loopne" || mn == "loopnz") {
                expect(n == 1 && o[0].t == ot::sym, (mn + " needs a label").c_str());
                const std::uint8_t op = mn == "loop" ? 0xE2 : (mn == "loope" || mn == "loopz") ? 0xE1 : (mn == "loopne" || mn == "loopnz") ? 0xE0 : 0xE3;
                if (mn == "jecxz") b(0x67);
                plain({op});
                return rel_label(o[0].label, 1);
            }
            if (mn.size() > 3 && mn.compare(0, 3, "set") == 0 && cc_table().count(mn.substr(3))) {
                expect(n == 1 && is_rm(o[0]), "setcc needs a byte register or memory operand");
                if (o[0].t == ot::reg) expect(o[0].size == 8, "setcc needs an 8-bit register");
                return modrm_op(0, false, false, {0x0F, static_cast<std::uint8_t>(0x90 + cc_table().at(mn.substr(3)))}, 0, o[0], 0, o[0].t == ot::reg && o[0].rex8);
            }
            if (mn.size() > 4 && mn.compare(0, 4, "cmov") == 0 && cc_table().count(mn.substr(4))) {
                expect(n == 2 && is_gpr(o[0]) && is_rm(o[1]), "cmovcc needs a register and a register/memory source");
                expect(o[0].size >= 16, "cmovcc needs a 16/32/64-bit register");
                if (o[1].t == ot::reg) expect(o[1].size == o[0].size, "operand size mismatch");
                else check_match(o[1], o[0].size);
                return modrm_op(0, o[0].size == 16, o[0].size == 64, {0x0F, static_cast<std::uint8_t>(0x40 + cc_table().at(mn.substr(4)))}, o[0].reg, o[1], 0);
            }
            if (mn == "bt" || mn == "bts" || mn == "btr" || mn == "btc") return enc_bt(mn, o);
            if (mn == "shld" || mn == "shrd") return enc_shift_double(mn == "shrd", o);
            if (mn == "bsf" || mn == "bsr" || mn == "popcnt" || mn == "lzcnt" || mn == "tzcnt") {
                expect(n == 2 && is_gpr(o[0]) && is_rm(o[1]) && o[0].size >= 16, (mn + " needs a register and a register/memory source").c_str());
                const std::uint8_t op = mn == "bsf" ? 0xBC : mn == "bsr" ? 0xBD : mn == "popcnt" ? 0xB8 : mn == "lzcnt" ? 0xBD : 0xBC;
                const std::uint8_t mand = (mn == "popcnt" || mn == "lzcnt" || mn == "tzcnt") ? 0xF3 : 0;
                return modrm_op(mand, o[0].size == 16, o[0].size == 64, {0x0F, op}, o[0].reg, o[1], 0);
            }
            if (mn == "bswap") {
                expect(n == 1 && is_gpr(o[0]) && (o[0].size == 32 || o[0].size == 64), "bswap needs a 32/64-bit register");
                return reg_op(false, o[0].size == 64, {0x0F, 0xC8}, o[0].reg);
            }
            if (mn == "cmpxchg" || mn == "xadd") {
                expect(n == 2 && is_rm(o[0]) && is_gpr(o[1]), (mn + " needs r/m, register").c_str());
                const int sz = o[1].size;
                check_match(o[0], sz);
                const std::uint8_t base = mn == "cmpxchg" ? 0xB0 : 0xC0;
                return modrm_op(0, sz == 16, sz == 64, {0x0F, static_cast<std::uint8_t>(sz == 8 ? base : base + 1)}, o[1].reg, o[0], 0, need_rex8(o[0], o[1]));
            }
            if (mn == "ldmxcsr" || mn == "stmxcsr") {
                expect(n == 1 && o[0].t == ot::mem, (mn + " needs a memory operand").c_str());
                return modrm_op(0, false, false, {0x0F, 0xAE}, mn == "ldmxcsr" ? 2 : 3, o[0], 0);
            }
            if (mn == "prefetcht0" || mn == "prefetcht1" || mn == "prefetcht2" || mn == "prefetchnta") {
                expect(n == 1 && o[0].t == ot::mem, (mn + " needs a memory operand").c_str());
                return modrm_op(0, false, false, {0x0F, 0x18}, mn == "prefetchnta" ? 0 : mn == "prefetcht0" ? 1 : mn == "prefetcht1" ? 2 : 3, o[0], 0);
            }
            if (enc_sse(mn, o)) return;

            throw std::runtime_error("unknown instruction '" + mn + "'");
        }

        // ---- label / symbol references -----------------------------------------------------
        void rel_label(const std::string& label, int bytes_) {
            const std::size_t at = here();
            imm_n(0, bytes_);
            fixups_.push_back({fix_kind::branch, at, bytes_, true, at + static_cast<std::size_t>(bytes_), label, {}, 0, lines[line_no].line});
        }

        void enc_call_jmp(bool is_call, std::vector<operand>& o) {
            expect(o.size() == 1, "call/jmp take one operand");
            const operand& t = o[0];
            if (t.t == ot::sym) {
                plain({static_cast<std::uint8_t>(is_call ? 0xE8 : 0xE9)});
                const std::size_t at = here();
                imm_n(0, 4);
                fixups_.push_back({is_call ? fix_kind::call : fix_kind::jmp, at, 4, true, at + 4, t.label, {}, 0, lines[line_no].line});
                return;
            }
            expect(is_rm(t), "call/jmp need a label, register or memory operand");
            if (t.t == ot::reg) expect(t.size == 64, "indirect call/jmp needs a 64-bit register");
            else expect(t.size == 0 || t.size == 64, "indirect call/jmp needs a qword memory operand");
            modrm_op(0, false, false, {0xFF}, is_call ? 2 : 4, t, 0);
        }

        // ---- mov ----------------------------------------------------------------------------
        void enc_mov(std::vector<operand>& o) {
            expect(o.size() == 2, "mov takes two operands");
            operand& d = o[0];
            operand& s = o[1];
            expect(d.t == ot::reg || d.t == ot::mem, "bad mov destination");

            if (d.t == ot::reg && !d.xmm && s.t == ot::imm) {
                const int sz = d.size;
                if (sz == 64) {
                    if (fits32i(s)) {
                        modrm_op(0, false, true, {0xC7}, 0, d, 4);
                        return emit_imm(s, 4);
                    }
                    reg_op(false, true, {0xB8}, d.reg);
                    return emit_imm(s, 8);
                }
                if (sz == 32) { reg_op(false, false, {0xB8}, d.reg); return emit_imm(s, 4); }
                if (sz == 16) { reg_op(true, false, {0xB8}, d.reg); return emit_imm(s, 2); }
                reg_op(false, false, {0xB0}, d.reg, d.rex8);
                return emit_imm(s, 1);
            }
            if (d.t == ot::mem && s.t == ot::imm) {
                const int sz = d.size;
                expect(sz != 0, "operand size needed: use byte/word/dword/qword");
                if (sz == 8) { modrm_op(0, false, false, {0xC6}, 0, d, 1); return emit_imm(s, 1); }
                if (sz == 16) { modrm_op(0, true, false, {0xC7}, 0, d, 2); return emit_imm(s, 2); }
                if (sz == 32) { modrm_op(0, false, false, {0xC7}, 0, d, 4); return emit_imm(s, 4); }
                expect(fits32i(s), "immediate does not fit in 32 bits (use a register)");
                modrm_op(0, false, true, {0xC7}, 0, d, 4);
                return emit_imm(s, 4);
            }
            if (d.t == ot::reg && d.xmm) { // mov xmm, ... is not valid; guide the user
                throw std::runtime_error("use movss/movsd/movq/movaps for xmm registers");
            }
            if (is_gpr(d) && is_rm(s)) { // load / reg-reg
                if (s.t == ot::reg) {
                    expect(!s.xmm, "use movq/movd to move between xmm and general registers");
                    expect(s.size == d.size, "operand size mismatch");
                    // GNU as encodes reg,reg as mov r/m, r (89)
                    const int sz = d.size;
                    return modrm_op(0, sz == 16, sz == 64, {static_cast<std::uint8_t>(sz == 8 ? 0x88 : 0x89)}, s.reg, d, 0, need_rex8(d, s));
                }
                check_match(s, d.size);
                const int sz = d.size;
                return modrm_op(0, sz == 16, sz == 64, {static_cast<std::uint8_t>(sz == 8 ? 0x8A : 0x8B)}, d.reg, s, 0, d.rex8);
            }
            if (d.t == ot::mem && is_gpr(s)) { // store
                check_match(d, s.size);
                const int sz = s.size;
                return modrm_op(0, sz == 16, sz == 64, {static_cast<std::uint8_t>(sz == 8 ? 0x88 : 0x89)}, s.reg, d, 0, s.rex8);
            }
            throw std::runtime_error("unsupported mov operands");
        }

        // ---- add/or/adc/sbb/and/sub/xor/cmp -------------------------------------------------
        void enc_alu(int idx, std::vector<operand>& o) {
            expect(o.size() == 2, "this instruction takes two operands");
            operand& d = o[0];
            operand& s = o[1];
            const std::uint8_t base = static_cast<std::uint8_t>(idx * 8);
            if (s.t == ot::imm) {
                expect(is_rm(d) && (d.t != ot::reg || is_gpr(d)), "bad destination");
                const int sz = need_size(d);
                expect(sz != 0, "operand size needed: use byte/word/dword/qword");
                check_match(d, sz);
                if (sz == 8) {
                    if (d.t == ot::reg && d.reg == 0 && !d.high8) { plain({static_cast<std::uint8_t>(base + 4)}); return emit_imm(s, 1); }
                    modrm_op(0, false, false, {0x80}, idx, d, 1, d.t == ot::reg && d.rex8);
                    return emit_imm(s, 1);
                }
                const int immsz = sz == 16 ? 2 : 4;
                if (fits8i(s)) {
                    modrm_op(0, sz == 16, sz == 64, {0x83}, idx, d, 1);
                    return emit_imm(s, 1);
                }
                expect(sz == 64 ? fits32i(s) : true, "immediate does not fit in 32 bits");
                if (d.t == ot::reg && d.reg == 0) { // accumulator short form
                    plain({static_cast<std::uint8_t>(base + 5)}, sz == 16, sz == 64);
                    return emit_imm(s, immsz);
                }
                modrm_op(0, sz == 16, sz == 64, {0x81}, idx, d, immsz);
                return emit_imm(s, immsz);
            }
            if (is_gpr(d) && is_rm(s) && !(s.t == ot::reg && !is_gpr(s))) {
                if (s.t == ot::reg) {
                    expect(s.size == d.size, "operand size mismatch");
                    const int sz = d.size;
                    return modrm_op(0, sz == 16, sz == 64, {static_cast<std::uint8_t>(base + (sz == 8 ? 0 : 1))}, s.reg, d, 0, need_rex8(d, s));
                }
                check_match(s, d.size);
                const int sz = d.size;
                return modrm_op(0, sz == 16, sz == 64, {static_cast<std::uint8_t>(base + (sz == 8 ? 2 : 3))}, d.reg, s, 0, d.rex8);
            }
            if (d.t == ot::mem && is_gpr(s)) {
                check_match(d, s.size);
                const int sz = s.size;
                return modrm_op(0, sz == 16, sz == 64, {static_cast<std::uint8_t>(base + (sz == 8 ? 0 : 1))}, s.reg, d, 0, s.rex8);
            }
            throw std::runtime_error("unsupported operands");
        }

        void enc_test(std::vector<operand>& o) {
            expect(o.size() == 2, "test takes two operands");
            operand& d = o[0];
            operand& s = o[1];
            if (s.t == ot::imm) {
                expect(is_rm(d), "bad test destination");
                const int sz = need_size(d);
                expect(sz != 0, "operand size needed: use byte/word/dword/qword");
                check_match(d, sz);
                if (sz == 8) {
                    if (d.t == ot::reg && d.reg == 0 && !d.high8) { plain({0xA8}); return emit_imm(s, 1); }
                    modrm_op(0, false, false, {0xF6}, 0, d, 1, d.t == ot::reg && d.rex8);
                    return emit_imm(s, 1);
                }
                const int immsz = sz == 16 ? 2 : 4;
                if (d.t == ot::reg && d.reg == 0) { plain({0xA9}, sz == 16, sz == 64); return emit_imm(s, immsz); }
                modrm_op(0, sz == 16, sz == 64, {0xF7}, 0, d, immsz);
                return emit_imm(s, immsz);
            }
            expect(is_rm(d) && is_gpr(s), "test needs r/m, register");
            if (d.t == ot::reg) expect(d.size == s.size, "operand size mismatch");
            else check_match(d, s.size);
            const int sz = s.size;
            modrm_op(0, sz == 16, sz == 64, {static_cast<std::uint8_t>(sz == 8 ? 0x84 : 0x85)}, s.reg, d, 0, need_rex8(d, s));
        }

        void enc_push(std::vector<operand>& o) {
            expect(o.size() == 1, "push takes one operand");
            operand& a = o[0];
            if (a.t == ot::reg) {
                expect(is_gpr(a) && a.size == 64, "push needs a 64-bit register");
                return reg_op(false, false, {0x50}, a.reg);
            }
            if (a.t == ot::imm) {
                if (fits8i(a)) { plain({0x6A}); return emit_imm(a, 1); }
                expect(fits32i(a), "push immediate does not fit in 32 bits");
                plain({0x68});
                return emit_imm(a, 4);
            }
            expect(a.t == ot::mem && (a.size == 0 || a.size == 64), "push needs a qword memory operand");
            modrm_op(0, false, false, {0xFF}, 6, a, 0);
        }

        void enc_pop(std::vector<operand>& o) {
            expect(o.size() == 1, "pop takes one operand");
            operand& a = o[0];
            if (a.t == ot::reg) {
                expect(is_gpr(a) && a.size == 64, "pop needs a 64-bit register");
                return reg_op(false, false, {0x58}, a.reg);
            }
            expect(a.t == ot::mem && (a.size == 0 || a.size == 64), "pop needs a qword memory operand");
            modrm_op(0, false, false, {0x8F}, 0, a, 0);
        }

        void enc_unary(const std::string& mn, std::vector<operand>& o) {
            expect(o.size() == 1 && is_rm(o[0]), (mn + " takes one register/memory operand").c_str());
            const int sz = need_size(o[0]);
            expect(sz != 0, "operand size needed: use byte/word/dword/qword");
            if (o[0].t == ot::reg) expect(is_gpr(o[0]), "general register expected");
            const bool incdec = mn == "inc" || mn == "dec";
            int ext;
            if (mn == "inc") ext = 0;
            else if (mn == "dec") ext = 1;
            else if (mn == "not") ext = 2;
            else if (mn == "neg") ext = 3;
            else if (mn == "mul") ext = 4;
            else if (mn == "div") ext = 6;
            else ext = 7;
            const std::uint8_t op8 = incdec ? 0xFE : 0xF6;
            const std::uint8_t op = incdec ? 0xFF : 0xF7;
            modrm_op(0, sz == 16, sz == 64, {sz == 8 ? op8 : op}, ext, o[0], 0, o[0].t == ot::reg && o[0].rex8);
        }

        void enc_imul(std::vector<operand>& o) {
            const std::size_t n = o.size();
            if (n == 1) {
                expect(is_rm(o[0]), "bad imul operand");
                const int sz = need_size(o[0]);
                expect(sz != 0, "operand size needed: use byte/word/dword/qword");
                return modrm_op(0, sz == 16, sz == 64, {static_cast<std::uint8_t>(sz == 8 ? 0xF6 : 0xF7)}, 5, o[0], 0, o[0].t == ot::reg && o[0].rex8);
            }
            expect((n == 2 || n == 3) && is_gpr(o[0]) && o[0].size >= 16, "imul reg, r/m[, imm]");
            const int sz = o[0].size;
            if (n == 2 && o[1].t != ot::imm) {
                expect(is_rm(o[1]), "bad imul source");
                if (o[1].t == ot::reg) expect(o[1].size == sz, "operand size mismatch");
                else check_match(o[1], sz);
                return modrm_op(0, sz == 16, sz == 64, {0x0F, 0xAF}, o[0].reg, o[1], 0);
            }
            operand src = n == 3 ? o[1] : o[0];
            const operand& im = n == 3 ? o[2] : o[1];
            expect(im.t == ot::imm && is_rm(src), "imul reg, r/m, imm");
            if (fits8i(im)) {
                modrm_op(0, sz == 16, sz == 64, {0x6B}, o[0].reg, src, 1);
                return emit_imm(im, 1);
            }
            const int immsz = sz == 16 ? 2 : 4;
            modrm_op(0, sz == 16, sz == 64, {0x69}, o[0].reg, src, immsz);
            emit_imm(im, immsz);
        }

        void enc_shift(int ext, std::vector<operand>& o) {
            expect(o.size() == 2 && is_rm(o[0]), "shift takes r/m and a count");
            const int sz = need_size(o[0]);
            expect(sz != 0, "operand size needed: use byte/word/dword/qword");
            const bool rex8 = o[0].t == ot::reg && o[0].rex8;
            if (o[1].t == ot::reg) {
                expect(o[1].reg == 1 && o[1].size == 8, "shift count register must be cl");
                return modrm_op(0, sz == 16, sz == 64, {static_cast<std::uint8_t>(sz == 8 ? 0xD2 : 0xD3)}, ext, o[0], 0, rex8);
            }
            expect(o[1].t == ot::imm, "shift count must be cl or an immediate");
            if (o[1].imm == 1)
                return modrm_op(0, sz == 16, sz == 64, {static_cast<std::uint8_t>(sz == 8 ? 0xD0 : 0xD1)}, ext, o[0], 0, rex8);
            modrm_op(0, sz == 16, sz == 64, {static_cast<std::uint8_t>(sz == 8 ? 0xC0 : 0xC1)}, ext, o[0], 1, rex8);
            imm_n(o[1].imm, 1);
        }

        void enc_movx(bool sign, std::vector<operand>& o) {
            expect(o.size() == 2 && is_gpr(o[0]) && is_rm(o[1]) && o[0].size >= 16, "movzx/movsx need a register and a smaller source");
            int ssz = need_size(o[1]);
            expect(ssz == 8 || ssz == 16, "source must be byte or word (add a size keyword)");
            expect(ssz < o[0].size, "source must be smaller than the destination");
            if (ssz == 16 && o[0].size != 32 && o[0].size != 64) throw std::runtime_error("bad movzx/movsx sizes");
            const std::uint8_t op = static_cast<std::uint8_t>(sign ? (ssz == 8 ? 0xBE : 0xBF) : (ssz == 8 ? 0xB6 : 0xB7));
            modrm_op(0, o[0].size == 16, o[0].size == 64, {0x0F, op}, o[0].reg, o[1], 0, o[1].t == ot::reg && o[1].rex8);
        }

        void enc_xchg(std::vector<operand>& o) {
            expect(o.size() == 2, "xchg takes two operands");
            operand* r = nullptr;
            operand* m = nullptr;
            if (is_gpr(o[1])) { r = &o[1]; m = &o[0]; }
            else if (is_gpr(o[0]) && o[1].t == ot::mem) { r = &o[0]; m = &o[1]; }
            expect(r && is_rm(*m), "xchg needs a register and r/m");
            if (m->t == ot::reg) expect(m->size == r->size, "operand size mismatch");
            else check_match(*m, r->size);
            const int sz = r->size;
            if (m->t == ot::reg && sz != 8) { // short accumulator form
                if (m->reg == 0 && sz != 32) { return reg_op(sz == 16, sz == 64, {0x90}, r->reg); }
                if (r->reg == 0 && sz != 32) { return reg_op(sz == 16, sz == 64, {0x90}, m->reg); }
                if (sz == 32 && (m->reg == 0 || r->reg == 0) && !(m->reg == 0 && r->reg == 0)) {
                    return reg_op(false, false, {0x90}, m->reg == 0 ? r->reg : m->reg);
                }
            }
            modrm_op(0, sz == 16, sz == 64, {static_cast<std::uint8_t>(sz == 8 ? 0x86 : 0x87)}, r->reg, *m, 0, need_rex8(*m, *r));
        }

        void enc_shift_double(bool right, std::vector<operand>& o) {
            expect(o.size() == 3 && is_rm(o[0]) && is_gpr(o[1]),
                   "shld/shrd take r/m, a register and a count");
            const int sz = need_size(o[0]);
            expect(sz >= 16, "operand size needed: use word/dword/qword");
            expect(o[1].size == sz, "operand size mismatch");
            if (o[2].t == ot::imm) {
                modrm_op(0, sz == 16, sz == 64, {0x0F, static_cast<std::uint8_t>(right ? 0xAC : 0xA4)},
                         o[1].reg, o[0], 1);
                return imm_n(o[2].imm, 1);
            }
            expect(o[2].t == ot::reg && o[2].reg == 1 && o[2].size == 8 && !o[2].rex8,
                   "shld/shrd count must be cl or an immediate");
            modrm_op(0, sz == 16, sz == 64, {0x0F, static_cast<std::uint8_t>(right ? 0xAD : 0xA5)},
                     o[1].reg, o[0], 0);
        }

        void enc_bt(const std::string& mn, std::vector<operand>& o) {
            expect(o.size() == 2 && is_rm(o[0]), (mn + " needs r/m and a bit index").c_str());
            const int ext = mn == "bt" ? 4 : mn == "bts" ? 5 : mn == "btr" ? 6 : 7;
            const int sz = need_size(o[0], &o[1]);
            expect(sz >= 16, "operand size needed: use word/dword/qword");
            if (o[1].t == ot::imm) {
                modrm_op(0, sz == 16, sz == 64, {0x0F, 0xBA}, ext, o[0], 1);
                return imm_n(o[1].imm, 1);
            }
            expect(is_gpr(o[1]), "bit index must be a register or immediate");
            const std::uint8_t op = static_cast<std::uint8_t>(mn == "bt" ? 0xA3 : mn == "bts" ? 0xAB : mn == "btr" ? 0xB3 : 0xBB);
            modrm_op(0, sz == 16, sz == 64, {0x0F, op}, o[1].reg, o[0], 0);
        }

        // ---- SSE ------------------------------------------------------------------------------
        bool enc_sse(const std::string& mn, std::vector<operand>& o) {
            const std::size_t n = o.size();
            auto xmm = [](const operand& a) { return a.t == ot::reg && a.xmm; };
            auto xmm_or_mem = [&](const operand& a) { return xmm(a) || a.t == ot::mem; };

            if (mn == "movaps" || mn == "movups" || mn == "movapd" || mn == "movupd" || mn == "movdqa" || mn == "movdqu" || mn == "movss" || mn == "movsd" || mn == "movlps" || mn == "movhps" || mn == "movlpd" || mn == "movhpd") {
                expect(n == 2 && (xmm(o[0]) || xmm(o[1])), (mn + " needs an xmm operand").c_str());
                std::uint8_t mand = 0, ld = 0, st = 0;
                if (mn == "movaps") { ld = 0x28; st = 0x29; }
                else if (mn == "movups") { ld = 0x10; st = 0x11; }
                else if (mn == "movapd") { mand = 0x66; ld = 0x28; st = 0x29; }
                else if (mn == "movupd") { mand = 0x66; ld = 0x10; st = 0x11; }
                else if (mn == "movdqa") { mand = 0x66; ld = 0x6F; st = 0x7F; }
                else if (mn == "movdqu") { mand = 0xF3; ld = 0x6F; st = 0x7F; }
                else if (mn == "movss") { mand = 0xF3; ld = 0x10; st = 0x11; }
                else if (mn == "movsd") { mand = 0xF2; ld = 0x10; st = 0x11; }
                else if (mn == "movlps") { ld = 0x12; st = 0x13; }
                else if (mn == "movhps") { ld = 0x16; st = 0x17; }
                else if (mn == "movlpd") { mand = 0x66; ld = 0x12; st = 0x13; }
                else { mand = 0x66; ld = 0x16; st = 0x17; }
                if (xmm(o[0]) && xmm_or_mem(o[1])) return modrm_op(mand, false, false, {0x0F, ld}, o[0].reg, o[1], 0), true;
                expect(o[0].t == ot::mem && xmm(o[1]), "bad operands");
                return modrm_op(mand, false, false, {0x0F, st}, o[1].reg, o[0], 0), true;
            }
            if (mn == "movd" || mn == "movq") {
                expect(n == 2, (mn + " takes two operands").c_str());
                const bool q = mn == "movq";
                if (xmm(o[0]) && (is_gpr(o[1]) || o[1].t == ot::mem) && !(q && o[1].t == ot::mem)) {
                    const int sz = is_gpr(o[1]) ? o[1].size : (o[1].size ? o[1].size : (q ? 64 : 32));
                    expect(sz == (q ? 64 : 32), "movd needs a 32-bit source, movq a 64-bit one");
                    return modrm_op(0x66, false, q, {0x0F, 0x6E}, o[0].reg, o[1], 0), true;
                }
                if (is_gpr(o[0]) && xmm(o[1])) {
                    expect(o[0].size == (q ? 64 : 32), "movd needs a 32-bit destination, movq a 64-bit one");
                    return modrm_op(0x66, false, q, {0x0F, 0x7E}, o[1].reg, o[0], 0), true;
                }
                if (q && xmm(o[0]) && xmm_or_mem(o[1])) return modrm_op(0xF3, false, false, {0x0F, 0x7E}, o[0].reg, o[1], 0), true;
                if (q && o[0].t == ot::mem && xmm(o[1])) return modrm_op(0x66, false, false, {0x0F, 0xD6}, o[1].reg, o[0], 0), true;
                if (!q && xmm(o[0]) && o[1].t == ot::mem) return modrm_op(0x66, false, false, {0x0F, 0x6E}, o[0].reg, o[1], 0), true;
                if (!q && o[0].t == ot::mem && xmm(o[1])) return modrm_op(0x66, false, false, {0x0F, 0x7E}, o[1].reg, o[0], 0), true;
                throw std::runtime_error("unsupported movd/movq operands");
            }
            if (mn == "cvtsi2ss" || mn == "cvtsi2sd") {
                expect(n == 2 && xmm(o[0]) && (is_gpr(o[1]) || o[1].t == ot::mem), (mn + " needs xmm, r/m").c_str());
                const int sz = need_size(o[1]);
                expect(sz == 32 || sz == 64, "source must be 32/64-bit (add dword/qword for memory)");
                return modrm_op(mn == "cvtsi2ss" ? 0xF3 : 0xF2, false, sz == 64, {0x0F, 0x2A}, o[0].reg, o[1], 0), true;
            }
            if (mn == "cvttss2si" || mn == "cvttsd2si" || mn == "cvtss2si" || mn == "cvtsd2si") {
                expect(n == 2 && is_gpr(o[0]) && xmm_or_mem(o[1]) && (o[0].size == 32 || o[0].size == 64), (mn + " needs a 32/64-bit register and an xmm source").c_str());
                const bool tt = mn[3] == 't';
                const bool ss = mn.find("ss") != std::string::npos;
                return modrm_op(ss ? 0xF3 : 0xF2, false, o[0].size == 64, {0x0F, static_cast<std::uint8_t>(tt ? 0x2C : 0x2D)}, o[0].reg, o[1], 0), true;
            }
            if (mn == "movmskps" || mn == "movmskpd" || mn == "pmovmskb") {
                expect(n == 2 && is_gpr(o[0]) && xmm(o[1]), (mn + " needs a register and an xmm register").c_str());
                return modrm_op(mn == "movmskps" ? 0 : 0x66, false, false, {0x0F, static_cast<std::uint8_t>(mn == "pmovmskb" ? 0xD7 : 0x50)}, o[0].reg, o[1], 0), true;
            }
            if (mn == "shufps" || mn == "shufpd" || mn == "pshufd" || mn == "pshufhw" || mn == "pshuflw") {
                expect(n == 3 && xmm(o[0]) && xmm_or_mem(o[1]) && o[2].t == ot::imm, (mn + " needs xmm, xmm/m, imm8").c_str());
                const std::uint8_t mand = mn == "shufps" ? 0 : mn == "pshufhw" ? 0xF3 : mn == "pshuflw" ? 0xF2 : 0x66;
                const std::uint8_t op = (mn == "shufps" || mn == "shufpd") ? 0xC6 : 0x70;
                modrm_op(mand, false, false, {0x0F, op}, o[0].reg, o[1], 1);
                return imm_n(o[2].imm, 1), true;
            }
            if (auto it = sse_table().find(mn); it != sse_table().end()) {
                expect(n == 2 && xmm(o[0]) && xmm_or_mem(o[1]), (mn + " needs xmm, xmm/m").c_str());
                return modrm_op(it->second.prefix, false, false, {0x0F, it->second.opcode}, o[0].reg, o[1], 0), true;
            }
            return false;
        }

        // ---------------------------------------------------------------- data directives
        void emit_data(int bytes_, cursor& c) {
            while (true) {
                if (c.end())
                    throw std::runtime_error("expected a value");
                if (c.cur().kind == tk::str) {
                    if (bytes_ != 1)
                        throw std::runtime_error("strings are only allowed in db / ascii / asciz");
                    for (char ch : c.cur().text)
                        b(static_cast<std::uint8_t>(ch));
                    ++c.i;
                }
                else {
                    const std::size_t start = c.i;
                    // validate shape now (labels allowed), evaluate in pass 2
                    cursor probe{c.toks, start};
                    parse_expr(probe, [](const std::string&) -> std::optional<std::int64_t> { return 0; }, 0);
                    fixup f;
                    f.kind = fix_kind::data;
                    f.at = here();
                    f.bytes = bytes_;
                    f.relative = false;
                    f.end = 0;
                    f.expr.assign(c.toks->begin() + static_cast<std::ptrdiff_t>(start), c.toks->begin() + static_cast<std::ptrdiff_t>(probe.i));
                    f.line = lines[line_no].line;
                    imm_n(0, bytes_);
                    fixups_.push_back(std::move(f));
                    c.i = probe.i;
                }
                if (c.is_punct(",")) { ++c.i; continue; }
                break;
            }
            if (!c.end())
                throw std::runtime_error("unexpected '" + c.cur().text + "'");
        }

        void emit_rel32(cursor& c) {
            if (c.end() || c.cur().kind != tk::id)
                throw std::runtime_error("rel32 needs a label or a function name");
            const std::string name = c.cur().text;
            ++c.i;
            std::int64_t addend = 0;
            if (c.is_punct("+") || c.is_punct("-")) {
                const bool neg = c.cur().text == "-";
                ++c.i;
                if (c.end() || c.cur().kind != tk::num)
                    throw std::runtime_error("rel32 needs a number after '+' or '-'");
                addend = c.cur().num;
                if (neg)
                    addend = -addend;
                ++c.i;
            }
            if (!c.end())
                throw std::runtime_error("unexpected '" + c.cur().text + "' after rel32");
            const std::size_t at = here();
            imm_n(0, 4);
            fixups_.push_back({fix_kind::rip, at, 4, true, at + 4, name, {}, addend, lines[line_no].line});
        }

        // ---------------------------------------------------------------- lines
        void assemble_line(line_t& ln) {
            cursor c{&ln.toks};
            line_prefixes.clear();

            // labels
            while (c.i + 1 < ln.toks.size() && c.cur().kind == tk::id && ln.toks[c.i + 1].kind == tk::punct && ln.toks[c.i + 1].text == ":") {
                const std::string name = c.cur().text;
                if (registers().count(lower(name)))
                    throw std::runtime_error("'" + name + "' is a register, not a label");
                if (labels_.count(name))
                    throw std::runtime_error("label '" + name + "' is defined twice");
                labels_[name] = here();
                c.i += 2;
            }
            if (c.end())
                return;

            if (c.cur().kind != tk::id)
                throw std::runtime_error("expected an instruction");
            std::string mn = lower(c.cur().text);
            ++c.i;

            // directives (with or without leading '.')
            const bool dotted = mn[0] == '.';
            const std::string dir = dotted ? mn.substr(1) : mn;
            if (dir == "db" || (dotted && dir == "byte")) return emit_data(1, c);
            if (dir == "dw" || (dotted && (dir == "word" || dir == "short"))) return emit_data(2, c);
            if (dir == "dd" || (dotted && (dir == "long" || dir == "int"))) return emit_data(4, c);
            if (dir == "dq" || (dotted && dir == "quad")) return emit_data(8, c);
            if (dir == "rel32") return emit_rel32(c);
            if (dir == "ascii" || dir == "asciz" || (dotted && dir == "string")) {
                while (!c.end()) {
                    if (c.cur().kind != tk::str)
                        throw std::runtime_error("expected a string");
                    for (char ch : c.cur().text) b(static_cast<std::uint8_t>(ch));
                    if (dir != "ascii") b(0);
                    ++c.i;
                    if (c.is_punct(",")) ++c.i;
                }
                return;
            }
            if (dir == "zero" || dir == "resb" || (dotted && (dir == "space" || dir == "skip"))) {
                const std::int64_t n = parse_const(c);
                if (n < 0 || n > (1 << 24)) throw std::runtime_error("bad size");
                for (std::int64_t i = 0; i < n; ++i) b(0);
                return;
            }
            if (dir == "align" || dir == "balign") {
                const std::int64_t a = parse_const(c);
                if (a <= 0 || (a & (a - 1))) throw std::runtime_error("alignment must be a power of two");
                std::int64_t fill = 0x90; // nop, which is what you want inside code; use `align 8, 0` for data
                if (c.is_punct(",")) {
                    ++c.i;
                    fill = parse_const(c);
                }
                // relative to the start of this block
                while (here() % static_cast<std::size_t>(a)) b(static_cast<std::uint8_t>(fill));
                return;
            }

            // prefixes
            while (true) {
                if (mn == "lock") line_prefixes.push_back(0xF0);
                else if (mn == "rep" || mn == "repe" || mn == "repz") line_prefixes.push_back(0xF3);
                else if (mn == "repne" || mn == "repnz") line_prefixes.push_back(0xF2);
                else break;
                if (c.end()) { // bare prefix line
                    throw std::runtime_error("prefix without an instruction");
                }
                mn = lower(c.cur().text);
                ++c.i;
            }

            std::vector<operand> ops;
            if (!c.end()) {
                while (true) {
                    ops.push_back(parse_operand(c));
                    if (c.is_punct(",")) { ++c.i; continue; }
                    break;
                }
                if (!c.end())
                    throw std::runtime_error("unexpected '" + c.cur().text + "'");
            }
            encode(mn, ops);
        }

        // ---------------------------------------------------------------- pass 2
        void resolve_fixups() {
            for (auto& f : fixups_) {
                try {
                    if (!f.relative) {
                        cursor c{&f.expr};
                        const value v = parse_expr(
                            c,
                            [&](const std::string& name) -> std::optional<std::int64_t> {
                                if (auto l = labels_.find(name); l != labels_.end()) return static_cast<std::int64_t>(l->second);
                                return std::nullopt;
                            },
                            0);
                        if (v.lab != 0)
                            throw std::runtime_error("a label's absolute address is not known here (use label differences, or [rip + label])");
                        for (int i = 0; i < f.bytes; ++i) out.bytes[f.at + static_cast<std::size_t>(i)] = static_cast<std::uint8_t>((static_cast<std::uint64_t>(v.v) >> (8 * i)) & 0xFF);
                        continue;
                    }
                    if (auto l = labels_.find(f.label); l != labels_.end()) {
                        const std::int64_t rel = static_cast<std::int64_t>(l->second) + f.addend - static_cast<std::int64_t>(f.end);
                        if (f.bytes == 1 && !fits8(rel))
                            throw std::runtime_error("jump to '" + f.label + "' is out of range");
                        for (int i = 0; i < f.bytes; ++i) out.bytes[f.at + static_cast<std::size_t>(i)] = static_cast<std::uint8_t>((static_cast<std::uint64_t>(rel) >> (8 * i)) & 0xFF);
                        continue;
                    }
                    if (is_function && is_function(f.label)) {
                        if (f.kind == fix_kind::call || f.kind == fix_kind::jmp) {
                            out.calls.push_back({f.label, f.at - 1, f.kind == fix_kind::jmp});
                        }
                        else if (f.kind == fix_kind::rip) {
                            // the end is carried: an immediate moves it
                            out.addresses.push_back({f.label, f.at, false, f.addend, f.end});
                        }
                        else {
                            throw std::runtime_error("only call and jmp can reach a function; use a label for conditional jumps");
                        }
                        continue;
                    }
                    throw std::runtime_error("unknown label or function '" + f.label + "'");
                }
                catch (const std::runtime_error& e) {
                    throw std::runtime_error("asm line " + std::to_string(f.line) + ": " + e.what());
                }
            }
        }
    };
} // namespace occult::x86_64
