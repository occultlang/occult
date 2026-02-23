#include "error.hpp"

namespace occult {
    static std::string strip_func_name(const char* full_name) {
        std::string s(full_name);
        const auto paren = s.find('(');
        if (paren == std::string::npos) {
            return s;
        }
        const auto sep = s.rfind("::", paren);
        if (sep == std::string::npos) {
            return s.substr(0, paren);
        }
        return s.substr(sep + 2, paren - sep - 2);
    }

    const char* parsing_error::what() const noexcept {
        message.clear();
        if (!source_file.empty()) {
            message += source_file;
            message += ':';
        }
        message += std::to_string(tk.line);
        message += ':';
        message += std::to_string(tk.column);
        message += ": ";

        message += "unexpected '";
        if (tk.tt == end_of_file_tt) {
            message += "end of file";
        }
        else if (tk.lexeme.empty()) {
            message += "<empty token>";
        }
        else {
            message += tk.lexeme;
        }
        message += "', expected '";
        message += expected;
        message += '\'';

        const std::string short_name = strip_func_name(func_name);
        if (!short_name.empty()) {
            message += "  (in ";
            message += short_name;
            message += ')';
        }

        return message.c_str();
    }

} // namespace occult
