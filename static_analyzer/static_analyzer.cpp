#include "static_analyzer.hpp"

namespace occultlang 
{
    bool static_analyzer::match(token_type type, std::optional<std::string> value)
    {
        if (tokens.empty()) return false;  

        if (value.has_value())
        {
            return tokens.front().get_type() == type && tokens.front().get_lexeme() == value.value();
        }
        else 
        {
            return tokens.front().get_type() == type;
        }
    }

    bool static_analyzer::match_and_consume(token_type type, std::optional<std::string> value)
    {
        if (match(type, value))
        {
            tokens.erase(tokens.begin());
            return true;
        }
        
        return false;
    }
    
    std::string static_analyzer::get_current_line_number() { return std::to_string(tokens.front().get_line()); }

    std::string static_analyzer::get_current_col_number() { return std::to_string(tokens.front().get_column()); }

    std::string& static_analyzer::get_error_list() { return error_list; }

    std::string static_analyzer::append_error(std::string msg)
    {
        return msg + " (line: " + get_current_line_number() + ", col: " + get_current_col_number() + ")\n";
    }

    void static_analyzer::analyze()
    {
        while(!tokens.empty())
        {
            if (match_and_consume(tk_keyword, "array") && 
            match_and_consume(tk_delimiter, ":") && 
            !match_and_consume(tk_identifier, std::nullopt))
            {
                error_list += append_error("Expected identifier after array declaration");
            }
            else if (match_and_consume(tk_keyword, "num") && 
            match_and_consume(tk_delimiter, ":") && 
            !match_and_consume(tk_identifier))
            {
                error_list += append_error("Expected identifier after number declaration");
            }
            else if (match_and_consume(tk_keyword, "rnum") && 
            match_and_consume(tk_delimiter, ":") && 
            !match_and_consume(tk_identifier))
            {
                error_list += append_error("Expected identifier after real number declaration");
            }
            else if (match_and_consume(tk_keyword, "bool") && 
            match_and_consume(tk_delimiter, ":") && 
            !match_and_consume(tk_identifier))
            {
                error_list += append_error("Expected identifier after boolean declaration");
            }
            else if (match_and_consume(tk_keyword, "str") && 
            match_and_consume(tk_delimiter, ":") && 
            !match_and_consume(tk_identifier))
            {
                error_list += append_error("Expected identifier after string declaration");
            }
            else if (match_and_consume(tk_keyword, "num") && match_and_consume(tk_keyword, "ptr") && 
            match_and_consume(tk_delimiter, ":") && 
            !match_and_consume(tk_identifier))
            {
                error_list += append_error("Expected identifier after number pointer declaration");
            }
            else if (match_and_consume(tk_keyword, "rnum") && match_and_consume(tk_keyword, "ptr") && 
            match_and_consume(tk_delimiter, ":") && 
            !match_and_consume(tk_identifier))
            {
                error_list += append_error("Expected identifier after real number pointer declaration");
            }
            else if (match_and_consume(tk_keyword, "str") && match_and_consume(tk_keyword, "ptr") && 
            match_and_consume(tk_delimiter, ":") && 
            !match_and_consume(tk_identifier))
            {
                error_list += append_error("Expected identifier after string pointer declaration");
            }
            else 
            {
                tokens.erase(tokens.begin());
            }
        }
    }
} // occultlang