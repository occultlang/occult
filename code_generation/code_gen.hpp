#pragma once
#include "../parser/parser.hpp"

namespace occultlang
{
    enum debug_level
    {
        all = 0,
        some = 1,
        none = 2
    };

    class code_gen
    {
        std::vector<std::string> symbols;
        int arr_count = 0;
        std::vector<std::pair<std::string, std::string>> array_types_and_symbols;
    public:
        ~code_gen() = default;

        template <typename CheckType, typename NodeType>
        std::pair<bool, std::shared_ptr<CheckType>> check_type(NodeType node, bool debug = false, int level = 2)
        {
            if (auto t = std::dynamic_pointer_cast<CheckType>(node); t != nullptr)
            {
                if (debug && level == 0)
                {
                    std::cout << "type: " << typeid(t).name() << std::endl;
                }
                return std::make_pair(true, t);
            }

            return std::make_pair(false, nullptr);
        }

        template <typename AstType, typename NodeType = std::shared_ptr<AstType>>
        std::string generate(NodeType root, bool debug = false, int level = 2, std::string curr_func_name = "")
        {
            std::string generated_source;

            for (int i = 0; i < root->get_children().size(); i++)
            {
                auto node = root->get_child(i);

                if (node == nullptr)
                {
                    node = root;
                }

                auto func_decl = check_type<occ_ast::function_declaration>(node);
                if (func_decl.first)
                {
                    int idx = 0;

                    auto func_name = check_type<occ_ast::identifier>(func_decl.second->get_child(idx++)).second->content;

                    for (auto &s : symbols)
                    {
                        if (s == func_name)
                        {
                            std::cout << "Symbol already exists" << std::endl;
                        }
                    }

                    symbols.push_back(func_name);

                    if (debug && level == 0)
                    {
                        std::cout << "idx value: " << idx << std::endl;
                    }

                    if (debug && (level == 1 || level == 0))
                    {
                        std::cout << "function_name: " << func_name << std::endl;
                    }

                    auto next = func_decl.second->get_child(idx); // type or arguments

                    std::vector<std::pair<std::string, std::string>> args;

                    auto next_typename = std::string();
                    while (func_decl.second->get_child(idx)->has_child())
                    {
                        next = func_decl.second->get_child(idx++);

                        if (auto n1 = check_type<occ_ast::num_declaration>(next); n1.first)
                        {
                            next_typename = n1.second->to_string();
                        }
                        else if (auto n2 = check_type<occ_ast::bool_declaration>(next); n2.first)
                        {
                            next_typename = n2.second->to_string();
                        }
                        else if (auto n3 = check_type<occ_ast::float_declaration>(next); n3.first)
                        {
                            next_typename = n3.second->to_string();
                        }
                        else if (auto n4 = check_type<occ_ast::string_declaration>(next); n4.first)
                        {
                            next_typename = n4.second->to_string();
                        }
                        else if (auto n5 = check_type<occ_ast::array_declaration>(next); n5.first)
                        {
                            next_typename = n5.second->to_string();
                        }
                        else if (auto n6 = check_type<occ_ast::num_ptr_declaration>(next); n6.first)
                        {
                            next_typename = n6.second->to_string();
                        }
                        else if (auto n7 = check_type<occ_ast::rnum_ptr_declaration>(next); n7.first)
                        {
                            next_typename = n7.second->to_string();
                        }
                        else if (auto n8 = check_type<occ_ast::str_ptr_declaration>(next); n8.first)
                        {
                            next_typename = n8.second->to_string();
                        }
                        else if (auto n9 = check_type<occ_ast::void_ptr_declaration>(next); n9.first)
                        {
                            next_typename = n9.second->to_string();
                        }

                        auto args_name = check_type<occ_ast::identifier>(next->get_child()).second->content;

                        if (debug && level == 0)
                        {
                            std::cout << "idx value: " << idx << std::endl;
                        }

                        if (debug && (level == 1 || level == 0))
                        {
                            std::cout << "args name: " << args_name << std::endl;
                            std::cout << "args type: " << next_typename << std::endl;
                        }

                        args.push_back(std::make_pair(next_typename, args_name)); // add to vector
                    }

                    if (debug && (level == 1 || level == 0))
                    {
                        std::cout << "args size: " << args.size() << std::endl;
                    }

                    next = func_decl.second->get_child(idx++); // func type

                    if (debug && level == 0)
                    {
                        std::cout << "idx value: " << idx << std::endl;
                    }

                    if (auto n1 = check_type<occ_ast::num_declaration>(next); n1.first)
                    {
                        next_typename = n1.second->to_string();
                    }
                    else if (auto n1 = check_type<occ_ast::float_declaration>(next); n1.first)
                    {
                        next_typename = n1.second->to_string();
                    }
                    else if (auto n2 = check_type<occ_ast::bool_declaration>(next); n2.first)
                    {
                        next_typename = n2.second->to_string();
                    }
                    else if (auto n3 = check_type<occ_ast::void_declaration>(next); n3.first)
                    {
                        next_typename = n3.second->to_string();
                    }
                    else if (auto n4 = check_type<occ_ast::string_declaration>(next); n4.first)
                    {
                        next_typename = n4.second->to_string();
                    }
                    else if (auto n5 = check_type<occ_ast::array_declaration>(next); n5.first)
                    {
                        next_typename = n5.second->to_string();
                    }
                    else if (auto n6 = check_type<occ_ast::num_ptr_declaration>(next); n6.first)
                    {
                        next_typename = n6.second->to_string();
                    }
                    else if (auto n7 = check_type<occ_ast::rnum_ptr_declaration>(next); n7.first)
                    {
                        next_typename = n7.second->to_string();
                    }
                    else if (auto n8 = check_type<occ_ast::str_ptr_declaration>(next); n8.first)
                    {
                        next_typename = n8.second->to_string();
                    }
                    else if (auto n9 = check_type<occ_ast::void_ptr_declaration>(next); n9.first)
                    {
                        next_typename = n9.second->to_string();
                    }

                    if (debug && (level == 1 || level == 0))
                    {
                        std::cout << "function_type: " << next_typename << std::endl;
                    }

                    // generate code

                    if (next_typename == "void_declaration")
                    {
                        generated_source += "void ";
                    }
                    else if (next_typename == "num_declaration")
                    {
                        generated_source += "long ";
                    }
                    else if (next_typename == "float_declaration")
                    {
                        generated_source += "double ";
                    }
                    else if (next_typename == "bool_declaration")
                    {
                        generated_source += "long ";
                    }
                    else if (next_typename == "string_declaration")
                    {
                        generated_source += "char* ";
                    }
                    else if (next_typename == "array_declaration")
                    {
                        generated_source += "dyn_array* ";
                    }
                    else if (next_typename == "num_ptr_declaration")
                    {
                        generated_source += "long* ";
                    }
                    else if (next_typename == "rnum_ptr_declaration")
                    {
                        generated_source += "double* ";
                    }
                    else if (next_typename == "str_ptr_declaration")
                    {
                        generated_source += "char** ";
                    }
                    else if (next_typename == "void_ptr_declaration")
                    {
                        generated_source += "void* ";
                    }

                    if (func_name == "main")
                        generated_source += func_name + "(int argc, char *argv[]";
                    else 
                        generated_source += func_name + "(";

                    for (int i = 0; i < args.size(); i++)
                    {
                        if (args[i].first == "num_declaration")
                        {
                            generated_source += "long ";
                        }
                        else if (args[i].first == "float_declaration")
                        {
                            generated_source += "double ";
                        }
                        else if (args[i].first == "bool_declaration")
                        {
                            generated_source += "long ";
                        }
                        else if (args[i].first == "string_declaration")
                        {
                            generated_source += "char* ";
                        }
                        else if (args[i].first == "array_declaration")
                        {
                            generated_source += "dyn_array* ";
                        }
                        else if (args[i].first == "num_ptr_declaration")
                        {
                            generated_source += "long* ";
                        }
                        else if (args[i].first == "rnum_ptr_declaration")
                        {
                            generated_source += "double* ";
                        }
                        else if (args[i].first == "str_ptr_declaration")
                        {
                            generated_source += "char** ";
                        }
                        else if (args[i].first == "void_ptr_declaration")
                        {
                            generated_source += "void* ";
                        }

                        generated_source += args[i].second;

                        if (i != args.size() - 1)
                        {
                            generated_source += ", ";
                        }
                    }

                    generated_source += ") {\n";

                    if (func_name == "main")
                    {
                        generated_source += "tgc_start(&gc, &argc);\n";
                    }

                    if (debug)
                    {
                        std::cout << "body_start: " << check_type<occ_ast::body_start>(func_decl.second->get_child(idx)).first << std::endl;
                    }

                    for (int idx2 = idx; idx2 < func_decl.second->get_children().size(); idx2++)
                    {
                        generated_source += generate<occ_ast::body_start>(func_decl.second->get_child(idx2), debug, level, func_name);
                    }

                    if (func_name == "main" && next_typename == "void_declaration") 
                    {
                        generated_source += "tgc_stop(&gc);\n";
                    }

                    generated_source += "}\n";
                }

                auto unsafe_decl = check_type<occ_ast::unsafe>(node);
                if (unsafe_decl.first)
                {
                    if (debug)
                        std::cout << "unsafe: " << unsafe_decl.first << std::endl;
                    if (auto a1 = check_type<occ_ast::unsafe>(node); a1.first)
                    {
                        generated_source += check_type<occ_ast::string_literal>(unsafe_decl.second->get_child()).second->content;
                    }
                }

                auto func_call = check_type<occ_ast::function_call>(node);
                if (func_call.first)
                {
                    if (debug)
                    {
                        std::cout << "func_call: " << func_call.first << std::endl;
                        std::cout << "func_call children: " << func_call.second->get_children().size() << std::endl;
                    }

                    for (int idx = 0; idx < func_call.second->get_children().size(); idx++) // we can remove the checking here and just do recursion...
                    {
                        if (check_type<occ_ast::identifier>(func_call.second->get_child(idx)).first)
                        {
                            auto x = check_type<occ_ast::identifier>(func_call.second->get_child(idx)).second;

                            generated_source += x->content;
                        }
                        else if (check_type<occ_ast::deref_ptr>(func_call.second->get_child(idx)).first) // todo fix this
                        {
                            generated_source += "*";
                        }
                        else if (check_type<occ_ast::string_literal>(func_call.second->get_child(idx)).first)
                        {
                            auto x = check_type<occ_ast::string_literal>(func_call.second->get_child(idx)).second;
                            generated_source += "\"" + x->content + "\"";
                        }
                        else if (check_type<occ_ast::delimiter_declaration>(func_call.second->get_child(idx)).first)
                        {
                            auto x = check_type<occ_ast::delimiter_declaration>(func_call.second->get_child(idx)).second;
                            generated_source += x->content;
                        }
                        else if (check_type<occ_ast::operator_declaration>(func_call.second->get_child(idx)).first)
                        {
                            auto x = check_type<occ_ast::operator_declaration>(func_call.second->get_child(idx)).second;
                            generated_source += x->content;
                        }
                        else if (check_type<occ_ast::number_literal>(func_call.second->get_child(idx)).first)
                        {
                            auto x = check_type<occ_ast::number_literal>(func_call.second->get_child(idx)).second;
                            generated_source += x->content;
                        }
                        else if (check_type<occ_ast::comma>(func_call.second->get_child(idx)).first)
                        {
                            auto x = check_type<occ_ast::comma>(func_call.second->get_child(idx)).second;
                            generated_source += ", ";
                        }
                        else if (check_type<occ_ast::float_literal>(func_call.second->get_child(idx)).first)
                        {
                            auto x = check_type<occ_ast::float_literal>(func_call.second->get_child(idx)).second;
                            generated_source += x->content;
                        }
                    }

                    generated_source += ";\n";
                }

                auto id = check_type<occ_ast::identifier>(node);
                if (id.first)
                {
                    if (debug)
                        std::cout << "identifier: " << id.first << std::endl;

                    if (auto a1 = check_type<occ_ast::identifier>(node); a1.first)
                    {
                        generated_source += a1.second->content;

                        if (a1.second->has_child())
                        {
                            if (auto child = check_type<occ_ast::assignment>(a1.second->get_child()); child.first)
                            {
                                if (debug)
                                    std::cout << "assignment" << std::endl;

                                generated_source += "=";
                                generated_source += generate<occ_ast::assignment>(a1.second->get_child());
                                generated_source += ";";
                            }
                        }
                    }
                }

                auto deref = check_type<occ_ast::deref_ptr>(node); // todo is make them equal
                if (deref.first)
                {
                    generated_source += "*";
                }

                auto arr_decl = check_type<occ_ast::array_declaration>(node); // TODO IS STORE TYPE AND USE LATER FOR DYNAMIC TYPES
                if (arr_decl.first)
                {
                    if (check_type<occ_ast::identifier>(arr_decl.second->get_child()).first) 
                    {
                        auto x = check_type<occ_ast::identifier>(arr_decl.second->get_child()).second;

                        if (check_type<occ_ast::function_call>(x->get_child()).first)
                        {
                            generated_source += "dyn_array* " + x->content + " = " + generate<occ_ast::function_call>(x);
                        }
                        else
                        {
                            generated_source += "dyn_array* " + x->content + " = " + generate<occ_ast::identifier>(arr_decl.second);
                        }

                        continue;
                    }
                    //std::cout << "Array declaration found" << std::endl;

                    std::string type;
                    std::shared_ptr<occ_ast::identifier> id;

                    if (auto n = check_type<occ_ast::num_declaration>(arr_decl.second->get_child()); n.first)
                    {
                        type = "long";
                        //std::cout << "Number declaration found" << std::endl;

                        id = check_type<occ_ast::identifier>(n.second->get_child()).second;
                    }
                    else if (auto b = check_type<occ_ast::bool_declaration>(arr_decl.second->get_child()); b.first)
                    {
                        type = "long";
                        //std::cout << "Boolean declaration found" << std::endl;

                        id = check_type<occ_ast::identifier>(b.second->get_child()).second;
                    }
                    else if (auto f = check_type<occ_ast::float_declaration>(arr_decl.second->get_child()); f.first)
                    {
                        type = "double";
                        //std::cout << "Float declaration found" << std::endl;

                        id = check_type<occ_ast::identifier>(f.second->get_child()).second;
                    }
                    else if (auto s = check_type<occ_ast::string_declaration>(arr_decl.second->get_child()); s.first)
                    {
                        type = "string"; // const char*
                        //std::cout << "String declaration found" << std::endl;

                        id = check_type<occ_ast::identifier>(s.second->get_child()).second;
                    }
                    else if (auto a = check_type<occ_ast::array_declaration>(arr_decl.second->get_child()); a.first)
                    {
                        type = "self";
                        //std::cout << "Array declaration found" << std::endl;

                        id = check_type<occ_ast::identifier>(a.second->get_child()).second;
                    }

                    generated_source += "dyn_array* ";
                    //std::cout << "Generating source code" << std::endl;

                    generated_source += id->content + " = create_array_" + type + "();\n"; // issues with other sizes other than 0

                    int size = 0;
                    std::string array_members = "";

                    int c_idx = 0;
  
                    c_idx = 0;
                    bool first = false;
                    while (c_idx < id->get_children().size())
                    {
                        auto c = id->get_child(c_idx);
                        
                        if (auto n = check_type<occ_ast::number_literal>(c); n.first)
                        {
                            size++;
                            array_members += "add_num(" + id->content + ", " + n.second->content + ");\n";
                        }
                        else if (auto s = check_type<occ_ast::string_literal>(c); s.first)
                        {
                            size++;
                            array_members += "add_str(" + id->content + ", \"" + s.second->content + "\");\n";
                        }
                        else if (auto f = check_type<occ_ast::float_literal>(c); f.first)
                        {
                            size++;
                            array_members += "add_rnum(" + id->content + ", " + f.second->content + ");\n";
                        }
                        else if (auto b = check_type<occ_ast::boolean_literal>(c); b.first)
                        {
                            size++;
                            if (b.second->content == "true")
                            {
                                array_members += "add_num(" + id->content + ", 1);\n";
                            }
                            else if (b.second->content == "false")
                            {
                                array_members += "add_num(" + id->content + ", 0);\n";
                            }
                        }
                        else if (auto ar = check_type<occ_ast::array_declaration>(c); ar.first)
                        {
                            size++;

                            array_members += "add_self(" + id->content + ", " + ar.second->content + ");\n"; // add nested name
                        }
                        else if (auto a = check_type<occ_ast::identifier>(c); a.first)
                        {
                            size++;
                            array_members += "DYN_VECTOR_PUSH_BACK(" + id->content + ", " + a.second->content + ");\n";
                        }
                        
                        c_idx++;
                    }

                    generated_source += array_members;

                    //std::cout << "Finished processing array declaration" << std::endl;
                }

                auto string_literal = check_type<occ_ast::string_literal>(node);
                if (string_literal.first)
                {
                    if (debug)
                        std::cout << "string_literal: " << string_literal.first << std::endl;
                    if (auto a1 = check_type<occ_ast::string_literal>(node); a1.first)
                    {
                        generated_source += "\"" + a1.second->content + "\"";
                    }
                }

                auto delimiter_decl = check_type<occ_ast::delimiter_declaration>(node);
                if (delimiter_decl.first)
                {
                    if (debug)
                        std::cout << "delimiter_declaration: " << delimiter_decl.first << std::endl;
                    if (auto a1 = check_type<occ_ast::delimiter_declaration>(node); a1.first)
                    {
                        generated_source += a1.second->content;
                    }
                }

                auto postfix_prefix = check_type<occ_ast::postfix_or_prefix>(node);
                if (postfix_prefix.first)
                {
                    if (debug)
                        std::cout << "postfix or prefix" << std::endl;
                    
                    for (int i = 0; i < postfix_prefix.second->get_children().size(); i++) 
                    {
                        auto child = postfix_prefix.second->get_child(i);

                        if (auto a1 = check_type<occ_ast::operator_declaration>(child); a1.first)
                        {
                            generated_source += a1.second->content;
                        }
                        else if (auto a2 = check_type<occ_ast::identifier>(child); a2.first)
                        {
                            generated_source += a2.second->content;
                        }
                    }

                    generated_source += ";\n";
                }

                auto operator_decl = check_type<occ_ast::operator_declaration>(node);
                if (operator_decl.first)
                {
                    if (debug)
                        std::cout << "operator_declaration: " << operator_decl.first << std::endl;
                    if (auto a1 = check_type<occ_ast::operator_declaration>(node); a1.first)
                    {
                        generated_source += a1.second->content;
                    }
                }

                auto break_decl = check_type<occ_ast::break_declaration>(node);
                if (break_decl.first)
                {
                    if (debug)
                        std::cout << "break_declaration: " << break_decl.first << std::endl;
                    if (auto a1 = check_type<occ_ast::break_declaration>(node); a1.first)
                    {
                        generated_source += "break;\n";
                    }
                }

                auto if_decl = check_type<occ_ast::if_declaration>(node);
                if (if_decl.first)
                {
                    generated_source += "if (";
                    // Assuming the condition is the first child
                    generated_source += generate<occ_ast::if_declaration>(if_decl.second);
                    generated_source += ")\n";
                    generated_source += "{\n";
                    for (int idx = 0; idx < if_decl.second->get_children().size(); idx++)
                    {
                        if (check_type<occ_ast::body_start>(if_decl.second->get_child(idx)).first)
                        {
                            generated_source += generate<occ_ast::body_start>(if_decl.second->get_child(idx));
                        }
                    }
                    generated_source += "}\n";
                }

                auto elseif_decl = check_type<occ_ast::elseif_declaration>(node);
                if (elseif_decl.first)
                {
                    generated_source += "else if (";
                    generated_source += generate<occ_ast::elseif_declaration>(elseif_decl.second);
                    generated_source += ")\n";
                    generated_source += "{\n";

                    for (int idx = 0; idx < elseif_decl.second->get_children().size(); idx++)
                    {
                        if (check_type<occ_ast::body_start>(elseif_decl.second->get_child(idx)).first)
                        {
                            generated_source += generate<occ_ast::body_start>(elseif_decl.second->get_child(idx));
                        }
                    }

                    generated_source += "}\n";
                }

                auto else_decl = check_type<occ_ast::else_declaration>(node);
                if (else_decl.first)
                {
                    if (debug)
                        std::cout << "else_decl: " << else_decl.first << std::endl;
                    if (auto a1 = check_type<occ_ast::else_declaration>(node); a1.first)
                    {
                        generated_source += "else {\n";

                        for (int idx = 0; idx < else_decl.second->get_children().size(); idx++)
                        {
                            generated_source += generate<occ_ast::body_start>(else_decl.second->get_child(idx));
                        }

                        generated_source += "}\n";
                    }
                }

                auto while_decl = check_type<occ_ast::while_declaration>(node);
                if (while_decl.first)
                {
                    if (debug)
                        std::cout << "while_declaration: " << while_decl.first << std::endl;

                    generated_source += "while (";
                    generated_source += generate<occ_ast::while_declaration>(while_decl.second);
                    generated_source += ")\n";
                    generated_source += "{\n";

                    for (int idx = 0; idx < while_decl.second->get_children().size(); idx++)
                    {
                        if (check_type<occ_ast::body_start>(while_decl.second->get_child(idx)).first)
                        {
                            generated_source += generate<occ_ast::body_start>(while_decl.second->get_child(idx));
                        }
                    }

                    generated_source += "}\n";
                }

                auto loop_decl = check_type<occ_ast::loop_declaration>(node);
                if (loop_decl.first)
                {
                    if (debug)
                        std::cout << "loop_declaration: " << loop_decl.first << std::endl;
                    if (auto a1 = check_type<occ_ast::loop_declaration>(node); a1.first)
                    {
                        generated_source += "while(1) {\n";

                        for (int idx = 0; idx < loop_decl.second->get_children().size(); idx++)
                        {
                            generated_source += generate<occ_ast::body_start>(loop_decl.second->get_child(idx));
                        }

                        generated_source += "}\n";
                    }
                }

                auto continue_decl = check_type<occ_ast::continue_declaration>(node);
                if (continue_decl.first)
                {
                    if (debug)
                        std::cout << "continue_declaration: " << continue_decl.first << std::endl;
                    if (auto a1 = check_type<occ_ast::continue_declaration>(node); a1.first)
                    {
                        generated_source += "continue;\n";
                    }
                }

                auto return_decl = check_type<occ_ast::return_declaration>(node);
                if (return_decl.first)
                {
                    if (debug)
                    {
                        std::cout << "return_decl: " << return_decl.first << std::endl;

                        std::cout << "return_decl children: " << return_decl.second->get_children().size() << std::endl;
                    }

                    if (curr_func_name == "main")
                    {
                        generated_source += "tgc_stop(&gc);\n";
                    }

                    generated_source += "return ";

                    generated_source += generate<occ_ast::return_declaration>(return_decl.second);

                    generated_source += ";\n";
                }

                auto num_literal = check_type<occ_ast::number_literal>(node);
                if (num_literal.first)
                {
                    if (debug)
                    {
                        std::cout << "num literal: " << num_literal.first << std::endl;
                    }
                    if (auto a1 = check_type<occ_ast::number_literal>(node); a1.first)
                    {
                        generated_source += a1.second->content;
                    }
                }

                auto num_decl = check_type<occ_ast::num_declaration>(node);
                if (num_decl.first)
                {
                    if (debug)
                    {
                        std::cout << "num_decl: " << num_decl.first << std::endl;
                    }
                    generated_source += "long ";
                    if (debug)
                    {
                        std::cout << "num_decl children: " << num_decl.second->get_children().size() << std::endl;
                    }

                    auto num_name = num_decl.second->get_child(0);

                    if (auto n1 = check_type<occ_ast::identifier>(num_name); n1.first)
                    {
                        if (debug)
                        {
                            std::cout << "num_name: " << n1.first << std::endl;
                        }

                        auto num_name_content = n1.second->content;

                        generated_source += num_name_content;

                        if (auto assignment = check_type<occ_ast::assignment>(num_decl.second->get_child(1)); assignment.first)
                        { // maybe add other assignments *= /= etc
                            if (debug)
                            {
                                std::cout << "assignment: " << assignment.first << std::endl;
                            }

                            generated_source += " = ";

                            for (int j = 0; j < assignment.second->get_children().size(); j++)
                            { // add other types (operators, delimiters, etc)
                                auto child_assignment = assignment.second->get_child(j);

                                if (auto a1 = check_type<occ_ast::number_literal>(child_assignment); a1.first)
                                {
                                    generated_source += a1.second->content;
                                }
                                else if (auto a2 = check_type<occ_ast::identifier>(child_assignment); a2.first)
                                {
                                    generated_source += a2.second->content;
                                }
                                else if (auto a3 = check_type<occ_ast::operator_declaration>(child_assignment); a3.first)
                                {
                                    generated_source += a3.second->content;
                                }
                                else if (auto a4 = check_type<occ_ast::delimiter_declaration>(child_assignment); a4.first)
                                {
                                    generated_source += a4.second->content;
                                }
                                else if (auto a5 = check_type<occ_ast::function_call>(child_assignment); a5.first)
                                {
                                    generated_source += generate<occ_ast::function_call>(a5.second);
                                }
                            }
                        }
                    }
                    else
                    {
                        if (debug)
                        {
                            std::cout << "num_name: " << n1.first << std::endl;
                        }
                    }

                    generated_source += ";\n";
                }

                auto float_decl = check_type<occ_ast::float_declaration>(node);
                if (float_decl.first)
                {
                    if (debug)
                    {
                        std::cout << "float_decl: " << float_decl.first << std::endl;
                    }
                    generated_source += "double ";
                    if (debug)
                    {
                        std::cout << "float_decl children: " << float_decl.second->get_children().size() << std::endl;
                    }

                    auto num_name = float_decl.second->get_child(0);

                    if (auto n1 = check_type<occ_ast::identifier>(num_name); n1.first)
                    {
                        if (debug)
                        {
                            std::cout << "float_name: " << n1.first << std::endl;
                        }

                        auto num_name_content = n1.second->content;

                        generated_source += num_name_content;

                        if (auto assignment = check_type<occ_ast::assignment>(float_decl.second->get_child(1)); assignment.first)
                        { // maybe add other assignments *= /= etc
                            if (debug)
                            {
                                std::cout << "assignment: " << assignment.first << std::endl;
                            }

                            generated_source += " = ";

                            for (int j = 0; j < assignment.second->get_children().size(); j++)
                            { // add other types (operators, delimiters, etc)
                                auto child_assignment = assignment.second->get_child(j);

                                if (auto a1 = check_type<occ_ast::float_literal>(child_assignment); a1.first)
                                {
                                    generated_source += a1.second->content;
                                }
                                else if (auto a2 = check_type<occ_ast::identifier>(child_assignment); a2.first)
                                {
                                    generated_source += a2.second->content;
                                }
                                else if (auto a3 = check_type<occ_ast::operator_declaration>(child_assignment); a3.first)
                                {
                                    generated_source += a3.second->content;
                                }
                                else if (auto a4 = check_type<occ_ast::delimiter_declaration>(child_assignment); a4.first)
                                {
                                    generated_source += a4.second->content;
                                }
                                else if (auto a5 = check_type<occ_ast::function_call>(child_assignment); a5.first)
                                {
                                    generated_source += generate<occ_ast::function_call>(a5.second);
                                }
                            }
                        }
                    }
                    else
                    {
                        if (debug)
                        {
                            std::cout << "float_name: " << n1.first << std::endl;
                        }
                    }

                    generated_source += ";\n";
                }

                auto bool_decl = check_type<occ_ast::bool_declaration>(node);
                if (bool_decl.first)
                {
                    if (debug)
                    {
                        std::cout << "bool_declaration: " << bool_decl.first << std::endl;
                    }
                    generated_source += "long ";
                    if (debug)
                    {
                        std::cout << "bool_declaration children: " << bool_decl.second->get_children().size() << std::endl;
                    }

                    auto num_name = bool_decl.second->get_child(0);

                    if (auto n1 = check_type<occ_ast::identifier>(num_name); n1.first)
                    {
                        if (debug)
                        {
                            std::cout << "bool_name: " << n1.first << std::endl;
                        }

                        auto num_name_content = n1.second->content;

                        generated_source += num_name_content;

                        if (auto assignment = check_type<occ_ast::assignment>(bool_decl.second->get_child(1)); assignment.first)
                        { // maybe add other assignments *= /= etc
                            if (debug)
                            {
                                std::cout << "assignment: " << assignment.first << std::endl;
                            }

                            generated_source += " = ";

                            for (int j = 0; j < assignment.second->get_children().size(); j++)
                            {
                                auto child_assignment = assignment.second->get_child(j);

                                if (auto a1 = check_type<occ_ast::boolean_literal>(child_assignment); a1.first)
                                {
                                    if (a1.second->content == "true")
                                    {
                                        generated_source += "1";
                                    }
                                    else if (a1.second->content == "false")
                                    {
                                        generated_source += "0";
                                    }
                                }
                                else if (auto a2 = check_type<occ_ast::identifier>(child_assignment); a2.first)
                                {
                                    generated_source += a2.second->content;
                                }
                                else if (auto a3 = check_type<occ_ast::operator_declaration>(child_assignment); a3.first)
                                {
                                    generated_source += a3.second->content;
                                }
                                else if (auto a4 = check_type<occ_ast::delimiter_declaration>(child_assignment); a4.first)
                                {
                                    generated_source += a4.second->content;
                                }
                                else if (auto a5 = check_type<occ_ast::function_call>(child_assignment); a5.first)
                                {
                                    generated_source += generate<occ_ast::function_call>(a5.second);
                                }
                            }
                        }
                    }
                    else
                    {
                        if (debug)
                        {
                            std::cout << "bool_name: " << n1.first << std::endl;
                        }
                    }

                    generated_source += ";\n";
                }

                auto string_decl = check_type<occ_ast::string_declaration>(node);
                if (string_decl.first)
                {
                    if (debug)
                    {
                        std::cout << "string_declaration: " << string_decl.first << std::endl;
                    }
                    generated_source += "char* ";
                    if (debug)
                    {
                        std::cout << "string_declaration children: " << string_decl.second->get_children().size() << std::endl;
                    }

                    auto num_name = string_decl.second->get_child(0);

                    if (auto n1 = check_type<occ_ast::identifier>(num_name); n1.first)
                    {
                        if (debug)
                        {
                            std::cout << "string_name: " << n1.first << std::endl;
                        }

                        auto num_name_content = n1.second->content;

                        generated_source += num_name_content;

                        if (auto assignment = check_type<occ_ast::assignment>(string_decl.second->get_child(1)); assignment.first)
                        {
                            if (debug)
                            {
                                std::cout << "assignment: " << assignment.first << std::endl;
                            }

                            generated_source += " = ";

                            for (int j = 0; j < assignment.second->get_children().size(); j++)
                            {
                                auto child_assignment = assignment.second->get_child(j);

                                if (auto a1 = check_type<occ_ast::string_literal>(child_assignment); a1.first)
                                {
                                    generated_source += "\"" + a1.second->content + "\"";
                                }
                                else if (auto a2 = check_type<occ_ast::identifier>(child_assignment); a2.first)
                                {
                                    generated_source += a2.second->content;
                                }
                                else if (auto a3 = check_type<occ_ast::operator_declaration>(child_assignment); a3.first)
                                {
                                    generated_source += a3.second->content;
                                }
                                else if (auto a4 = check_type<occ_ast::delimiter_declaration>(child_assignment); a4.first)
                                {
                                    generated_source += a4.second->content;
                                }
                                else if (auto a5 = check_type<occ_ast::function_call>(child_assignment); a5.first)
                                {
                                    generated_source += generate<occ_ast::function_call>(a5.second);
                                }
                            }
                        }
                    }
                    else
                    {
                        if (debug)
                        {
                            std::cout << "string_name: " << n1.first << std::endl;
                        }
                    }

                    generated_source += ";\n";
                }

                auto num_ptr_decl = check_type<occ_ast::num_ptr_declaration>(node);
                if (num_ptr_decl.first)
                {
                    if (debug)
                    {
                        std::cout << "num_pointer_declaration: " << num_ptr_decl.first << std::endl;
                    }
                    generated_source += "long* ";
                    if (debug)
                    {
                        std::cout << "num_pointer_declaration children: " << num_ptr_decl.second->get_children().size() << std::endl;
                    }

                    auto num_name = num_ptr_decl.second->get_child(0);

                    if (auto n1 = check_type<occ_ast::identifier>(num_name); n1.first)
                    {
                        if (debug)
                        {
                            std::cout << "num_ptr_name: " << n1.first << std::endl;
                        }

                        auto num_name_content = n1.second->content;

                        generated_source += num_name_content;

                        if (auto assignment = check_type<occ_ast::assignment>(num_ptr_decl.second->get_child(1)); assignment.first)
                        {
                            if (debug)
                            {
                                std::cout << "assignment: " << assignment.first << std::endl;
                            }

                            generated_source += " = ";

                            for (int j = 0; j < assignment.second->get_children().size(); j++)
                            {
                                auto child_assignment = assignment.second->get_child(j);

                                if (auto a1 = check_type<occ_ast::number_literal>(child_assignment); a1.first)
                                {
                                    generated_source += a1.second->content;
                                }
                                else if (auto a2 = check_type<occ_ast::identifier>(child_assignment); a2.first)
                                {
                                    generated_source += a2.second->content;
                                }
                                else if (auto a3 = check_type<occ_ast::operator_declaration>(child_assignment); a3.first)
                                {
                                    generated_source += a3.second->content;
                                }
                                else if (auto a4 = check_type<occ_ast::delimiter_declaration>(child_assignment); a4.first)
                                {
                                    generated_source += a4.second->content;
                                }
                                else if (auto a5 = check_type<occ_ast::function_call>(child_assignment); a5.first)
                                {
                                    generated_source += generate<occ_ast::function_call>(a5.second);
                                }
                            }
                        }
                    }
                    else
                    {
                        if (debug)
                        {
                            std::cout << "num_ptr_name: " << n1.first << std::endl;
                        }
                    }

                    generated_source += ";\n";
                }

                auto float_ptr_decl = check_type<occ_ast::rnum_ptr_declaration>(node);

                if (float_ptr_decl.first)
                {
                    if (debug)
                    {
                        std::cout << "float_pointer_declaration: " << float_ptr_decl.first << std::endl;
                    }
                    generated_source += "double* ";
                    if (debug)
                    {
                        std::cout << "float_pointer_declaration children: " << float_ptr_decl.second->get_children().size() << std::endl;
                    }

                    auto num_name = float_ptr_decl.second->get_child(0);

                    if (auto n1 = check_type<occ_ast::identifier>(num_name); n1.first)
                    {
                        if (debug)
                        {
                            std::cout << "float_ptr_name: " << n1.first << std::endl;
                        }

                        auto num_name_content = n1.second->content;

                        generated_source += num_name_content;

                        if (auto assignment = check_type<occ_ast::assignment>(float_ptr_decl.second->get_child(1)); assignment.first)
                        {
                            if (debug)
                            {
                                std::cout << "assignment: " << assignment.first << std::endl;
                            }

                            generated_source += " = ";

                            for (int j = 0; j < assignment.second->get_children().size(); j++)
                            {
                                auto child_assignment = assignment.second->get_child(j);

                                if (auto a1 = check_type<occ_ast::float_literal>(child_assignment); a1.first)
                                {
                                    generated_source += a1.second->content;
                                }
                                else if (auto a2 = check_type<occ_ast::identifier>(child_assignment); a2.first)
                                {
                                    generated_source += a2.second->content;
                                }
                                else if (auto a3 = check_type<occ_ast::operator_declaration>(child_assignment); a3.first)
                                {
                                    generated_source += a3.second->content;
                                }
                                else if (auto a4 = check_type<occ_ast::delimiter_declaration>(child_assignment); a4.first)
                                {
                                    generated_source += a4.second->content;
                                }
                                else if (auto a5 = check_type<occ_ast::function_call>(child_assignment); a5.first)
                                {
                                    generated_source += generate<occ_ast::function_call>(a5.second);
                                }
                            }
                        }
                    }
                    else
                    {
                        if (debug)
                        {
                            std::cout << "float_ptr_name: " << n1.first << std::endl;
                        }
                    }

                    generated_source += ";\n";
                }

                auto string_ptr_decl = check_type<occ_ast::str_ptr_declaration>(node);
                if (string_ptr_decl.first)
                {
                    if (debug)
                    {
                        std::cout << "str_pointer_declaration: " << string_ptr_decl.first << std::endl;
                    }
                    generated_source += "char** ";
                    if (debug)
                    {
                        std::cout << "str_pointer_declaration children: " << string_ptr_decl.second->get_children().size() << std::endl;
                    }

                    auto num_name = string_ptr_decl.second->get_child(0);

                    if (auto n1 = check_type<occ_ast::identifier>(num_name); n1.first)
                    {
                        if (debug)
                        {
                            std::cout << "str_ptr_name: " << n1.first << std::endl;
                        }

                        auto num_name_content = n1.second->content;

                        generated_source += num_name_content;

                        if (auto assignment = check_type<occ_ast::assignment>(string_ptr_decl.second->get_child(1)); assignment.first)
                        {
                            if (debug)
                            {
                                std::cout << "assignment: " << assignment.first << std::endl;
                            }

                            generated_source += " = ";

                            for (int j = 0; j < assignment.second->get_children().size(); j++)
                            {
                                auto child_assignment = assignment.second->get_child(j);

                                if (auto a1 = check_type<occ_ast::string_literal>(child_assignment); a1.first)
                                {
                                    generated_source += "\"" + a1.second->content + "\"";
                                }
                                else if (auto a2 = check_type<occ_ast::identifier>(child_assignment); a2.first)
                                {
                                    generated_source += a2.second->content;
                                }
                                else if (auto a3 = check_type<occ_ast::operator_declaration>(child_assignment); a3.first)
                                {
                                    generated_source += a3.second->content;
                                }
                                else if (auto a4 = check_type<occ_ast::delimiter_declaration>(child_assignment); a4.first)
                                {
                                    generated_source += a4.second->content;
                                }
                                else if (auto a5 = check_type<occ_ast::function_call>(child_assignment); a5.first)
                                {
                                    generated_source += generate<occ_ast::function_call>(a5.second);
                                }
                            }
                        }
                    }
                    else
                    {
                        if (debug)
                        {
                            std::cout << "str_ptr_name: " << n1.first << std::endl;
                        }
                    }

                    generated_source += ";\n";
                }

                auto void_ptr_decl = check_type<occ_ast::void_ptr_declaration>(node);
                if (void_ptr_decl.first)
                {
                    if (debug)
                    {
                        std::cout << "void_pointer_declaration: " << void_ptr_decl.first << std::endl;
                    }
                    generated_source += "void* ";
                    if (debug)
                    {
                        std::cout << "void_pointer_declaration children: " << void_ptr_decl.second->get_children().size() << std::endl;
                    }

                    auto num_name = void_ptr_decl.second->get_child(0);

                    if (auto n1 = check_type<occ_ast::identifier>(num_name); n1.first)
                    {
                        if (debug)
                        {
                            std::cout << "void_ptr_name: " << n1.first << std::endl;
                        }

                        auto num_name_content = n1.second->content;

                        generated_source += num_name_content;

                        if (auto assignment = check_type<occ_ast::assignment>(void_ptr_decl.second->get_child(1)); assignment.first)
                        {
                            if (debug)
                            {
                                std::cout << "assignment: " << assignment.first << std::endl;
                            }

                            generated_source += " = ";

                            for (int j = 0; j < assignment.second->get_children().size(); j++)
                            {
                                auto child_assignment = assignment.second->get_child(j);

                                if (auto a1 = check_type<occ_ast::string_literal>(child_assignment); a1.first)
                                {
                                    generated_source += "\"" + a1.second->content + "\"";
                                }
                                else if (auto a6 = check_type<occ_ast::number_literal>(child_assignment); a6.first)
                                {
                                    generated_source += a1.second->content;
                                }
                                else if (auto a2 = check_type<occ_ast::identifier>(child_assignment); a2.first)
                                {
                                    generated_source += a2.second->content;
                                }
                                else if (auto a3 = check_type<occ_ast::operator_declaration>(child_assignment); a3.first)
                                {
                                    generated_source += a3.second->content;
                                }
                                else if (auto a4 = check_type<occ_ast::delimiter_declaration>(child_assignment); a4.first)
                                {
                                    generated_source += a4.second->content;
                                }
                                else if (auto a5 = check_type<occ_ast::function_call>(child_assignment); a5.first)
                                {
                                    generated_source += generate<occ_ast::function_call>(a5.second);
                                }
                            }
                        }
                    }
                    else
                    {
                        if (debug)
                        {
                            std::cout << "void_ptr_name: " << n1.first << std::endl;
                        }
                    }

                    generated_source += ";\n";
                }
            }

            return generated_source;
        }

            std::string lib = R"(
#include <setjmp.h>

typedef unsigned int uintptr_t; 
typedef uintptr_t size_t; 

#ifndef NULL
#define NULL ((void*)0)
#endif

#define UINTPTR_MAX 0xffffffff

enum {
  TGC_MARK = 0x01,
  TGC_ROOT = 0x02,
  TGC_LEAF = 0x04
};

typedef struct {
  void *ptr;
  int flags;
  size_t size, hash;
  void (*dtor)(void*);
} tgc_ptr_t;

typedef struct {
  void *bottom;
  int paused;
  uintptr_t minptr, maxptr;
  tgc_ptr_t *items, *frees;
  double loadfactor, sweepfactor;
  size_t nitems, nslots, mitems, nfrees;
} tgc_t;

void tgc_start(tgc_t *gc, void *stk);
void tgc_stop(tgc_t *gc);
void tgc_pause(tgc_t *gc);
void tgc_resume(tgc_t *gc);
void tgc_run(tgc_t *gc);

void *tgc_alloc(tgc_t *gc, size_t size);
void *tgc_calloc(tgc_t *gc, size_t num, size_t size);
void *tgc_realloc(tgc_t *gc, void *ptr, size_t size);
void tgc_free(tgc_t *gc, void *ptr);

void *tgc_alloc_opt(tgc_t *gc, size_t size, int flags, void(*dtor)(void*));
void *tgc_calloc_opt(tgc_t *gc, size_t num, size_t size, int flags, void(*dtor)(void*));

void tgc_set_dtor(tgc_t *gc, void *ptr, void(*dtor)(void*));
void tgc_set_flags(tgc_t *gc, void *ptr, int flags);
int tgc_get_flags(tgc_t *gc, void *ptr);
void(*tgc_get_dtor(tgc_t *gc, void *ptr))(void*);
size_t tgc_get_size(tgc_t *gc, void *ptr);

static size_t tgc_hash(void *ptr) {
    uintptr_t ad = (uintptr_t) ptr;
    return (size_t) ((13*ad) ^ (ad >> 15));
}

static size_t tgc_probe(tgc_t* gc, size_t i, size_t h) {
  long v = i - (h-1);
  if (v < 0) { v = gc->nslots + v; }
  return v;
}

static tgc_ptr_t *tgc_get_ptr(tgc_t *gc, void *ptr) {
  size_t i, j, h;
  i = tgc_hash(ptr) % gc->nslots; j = 0;
  while (1) {
    h = gc->items[i].hash;
    if (h == 0 || j > tgc_probe(gc, i, h)) { return NULL; }
    if (gc->items[i].ptr == ptr) { return &gc->items[i]; }
    i = (i+1) % gc->nslots; j++;
  }
  return NULL;
}

static void tgc_add_ptr(
  tgc_t *gc, void *ptr, size_t size, 
  int flags, void(*dtor)(void*)) {

  tgc_ptr_t item, tmp;
  size_t h, p, i, j;

  i = tgc_hash(ptr) % gc->nslots; j = 0;
  
  item.ptr = ptr;
  item.flags = flags;
  item.size = size;
  item.hash = i+1;
  item.dtor = dtor;
  
  while (1) {
    h = gc->items[i].hash;
    if (h == 0) { gc->items[i] = item; return; }
    if (gc->items[i].ptr == item.ptr) { return; }
    p = tgc_probe(gc, i, h);
    if (j >= p) {
      tmp = gc->items[i];
      gc->items[i] = item;
      item = tmp;
      j = p;
    }
    i = (i+1) % gc->nslots; j++;
  }
  
}

static void tgc_rem_ptr(tgc_t *gc, void *ptr) {

  size_t i, j, h, nj, nh;

  if (gc->nitems == 0) { return; }
  
  for (i = 0; i < gc->nfrees; i++) {
    if (gc->frees[i].ptr == ptr) { gc->frees[i].ptr = NULL; }
  }
  
  i = tgc_hash(ptr) % gc->nslots; j = 0;
  
  while (1) {
    h = gc->items[i].hash;
    if (h == 0 || j > tgc_probe(gc, i, h)) { return; }
    if (gc->items[i].ptr == ptr) {
      memset(&gc->items[i], 0, sizeof(tgc_ptr_t));
      j = i;
      while (1) { 
        nj = (j+1) % gc->nslots;
        nh = gc->items[nj].hash;
        if (nh != 0 && tgc_probe(gc, nj, nh) > 0) {
          memcpy(&gc->items[ j], &gc->items[nj], sizeof(tgc_ptr_t));
          memset(&gc->items[nj],              0, sizeof(tgc_ptr_t));
          j = nj;
        } else {
          break;
        }  
      }
      gc->nitems--;
      return;
    }
    i = (i+1) % gc->nslots; j++;
  }
  
}


enum {
  TGC_PRIMES_COUNT = 24
};

static const size_t tgc_primes[TGC_PRIMES_COUNT] = {
  0,       1,       5,       11,
  23,      53,      101,     197,
  389,     683,     1259,    2417,
  4733,    9371,    18617,   37097,
  74093,   148073,  296099,  592019,
  1100009, 2200013, 4400021, 8800019
};

static size_t tgc_ideal_size(tgc_t* gc, size_t size) {
  size_t i, last;
  size = (size_t)((double)(size+1) / gc->loadfactor);
  for (i = 0; i < TGC_PRIMES_COUNT; i++) {
    if (tgc_primes[i] >= size) { return tgc_primes[i]; }
  }
  last = tgc_primes[TGC_PRIMES_COUNT-1];
  for (i = 0;; i++) {
    if (last * i >= size) { return last * i; }
  }
  return 0;
}

static int tgc_rehash(tgc_t* gc, size_t new_size) {

  size_t i;
  tgc_ptr_t *old_items = gc->items;
  size_t old_size = gc->nslots;
  
  gc->nslots = new_size;
  gc->items = calloc(gc->nslots, sizeof(tgc_ptr_t));
  
  if (gc->items == NULL) {
    gc->nslots = old_size;
    gc->items = old_items;
    return 0;
  }
  
  for (i = 0; i < old_size; i++) {
    if (old_items[i].hash != 0) {
      tgc_add_ptr(gc, 
        old_items[i].ptr,   old_items[i].size, 
        old_items[i].flags, old_items[i].dtor);
    }
  }
  
  free(old_items);
  
  return 1;
}

static int tgc_resize_more(tgc_t *gc) {
  size_t new_size = tgc_ideal_size(gc, gc->nitems);  
  size_t old_size = gc->nslots;
  return (new_size > old_size) ? tgc_rehash(gc, new_size) : 1;
}

static int tgc_resize_less(tgc_t *gc) {
  size_t new_size = tgc_ideal_size(gc, gc->nitems);  
  size_t old_size = gc->nslots;
  return (new_size < old_size) ? tgc_rehash(gc, new_size) : 1;
}

static void tgc_mark_ptr(tgc_t *gc, void *ptr) {

  size_t i, j, h, k;
  
  if ((uintptr_t)ptr < gc->minptr 
  ||  (uintptr_t)ptr > gc->maxptr) { return; }
  
  i = tgc_hash(ptr) % gc->nslots; j = 0;
  
  while (1) {
    h = gc->items[i].hash;
    if (h == 0 || j > tgc_probe(gc, i, h)) { return; }
    if (ptr == gc->items[i].ptr) {
      if (gc->items[i].flags & TGC_MARK) { return; }
      gc->items[i].flags |= TGC_MARK;
      if (gc->items[i].flags & TGC_LEAF) { return; }
      for (k = 0; k < gc->items[i].size/sizeof(void*); k++) {
        tgc_mark_ptr(gc, ((void**)gc->items[i].ptr)[k]);
      }
      return;
    }
    i = (i+1) % gc->nslots; j++;
  }
  
}

static void tgc_mark_stack(tgc_t *gc) {
  
  void *stk, *bot, *top, *p;
  bot = gc->bottom; top = &stk;
  
  if (bot == top) { return; }
  
  if (bot < top) {
    for (p = top; p >= bot; p = ((char*)p) - sizeof(void*)) {
      tgc_mark_ptr(gc, *((void**)p));
    }
  }
  
  if (bot > top) {
    for (p = top; p <= bot; p = ((char*)p) + sizeof(void*)) {
      tgc_mark_ptr(gc, *((void**)p));
    }
  }
  
}

static void tgc_mark(tgc_t *gc) {
  
  size_t i, k;
  jmp_buf env;
  void (*volatile mark_stack)(tgc_t*) = tgc_mark_stack;
  
  if (gc->nitems == 0) { return; }
  
  for (i = 0; i < gc->nslots; i++) {
    if (gc->items[i].hash ==        0) { continue; }
    if (gc->items[i].flags & TGC_MARK) { continue; }
    if (gc->items[i].flags & TGC_ROOT) {
      gc->items[i].flags |= TGC_MARK;
      if (gc->items[i].flags & TGC_LEAF) { continue; }
      for (k = 0; k < gc->items[i].size/sizeof(void*); k++) {
        tgc_mark_ptr(gc, ((void**)gc->items[i].ptr)[k]);
      }
      continue;
    }
  }
  
  memset(&env, 0, sizeof(jmp_buf));
  setjmp(env);
  mark_stack(gc);

}

void tgc_sweep(tgc_t *gc) {
  
  size_t i, j, k, nj, nh;
  
  if (gc->nitems == 0) { return; }
  
  gc->nfrees = 0;
  for (i = 0; i < gc->nslots; i++) {
    if (gc->items[i].hash ==        0) { continue; }
    if (gc->items[i].flags & TGC_MARK) { continue; }
    if (gc->items[i].flags & TGC_ROOT) { continue; }
    gc->nfrees++;
  }

  gc->frees = realloc(gc->frees, sizeof(tgc_ptr_t) * gc->nfrees);
  if (gc->frees == NULL) { return; }
  
  i = 0; k = 0;
  while (i < gc->nslots) {
    if (gc->items[i].hash ==        0) { i++; continue; }
    if (gc->items[i].flags & TGC_MARK) { i++; continue; }
    if (gc->items[i].flags & TGC_ROOT) { i++; continue; }
    
    gc->frees[k] = gc->items[i]; k++;
    memset(&gc->items[i], 0, sizeof(tgc_ptr_t));
    
    j = i;
    while (1) { 
      nj = (j+1) % gc->nslots;
      nh = gc->items[nj].hash;
      if (nh != 0 && tgc_probe(gc, nj, nh) > 0) {
        memcpy(&gc->items[ j], &gc->items[nj], sizeof(tgc_ptr_t));
        memset(&gc->items[nj],              0, sizeof(tgc_ptr_t));
        j = nj;
      } else {
        break;
      }  
    }
    gc->nitems--;
  }
  
  for (i = 0; i < gc->nslots; i++) {
    if (gc->items[i].hash == 0) { continue; }
    if (gc->items[i].flags & TGC_MARK) {
      gc->items[i].flags &= ~TGC_MARK;
    }
  }
  
  tgc_resize_less(gc);
  
  gc->mitems = gc->nitems + (size_t)(gc->nitems * gc->sweepfactor) + 1;
  
  for (i = 0; i < gc->nfrees; i++) {
    if (gc->frees[i].ptr) {
      if (gc->frees[i].dtor) { gc->frees[i].dtor(gc->frees[i].ptr); }
      free(gc->frees[i].ptr);
    }
  }
  
  free(gc->frees);
  gc->frees = NULL;
  gc->nfrees = 0;
  
}

void tgc_start(tgc_t *gc, void *stk) {
  gc->bottom = stk;
  gc->paused = 0;
  gc->nitems = 0;
  gc->nslots = 0;
  gc->mitems = 0;
  gc->nfrees = 0;
  gc->maxptr = 0;
  gc->items = NULL;
  gc->frees = NULL;
  gc->minptr = UINTPTR_MAX;
  gc->loadfactor = 0.9;
  gc->sweepfactor = 0.5;
}

void tgc_stop(tgc_t *gc) {
  tgc_sweep(gc);
  free(gc->items);
  free(gc->frees);
}

void tgc_pause(tgc_t *gc) {
  gc->paused = 1;
}

void tgc_resume(tgc_t *gc) {
  gc->paused = 0;
}

void tgc_run(tgc_t *gc) {
  tgc_mark(gc);
  tgc_sweep(gc);
}

static void *tgc_add(
  tgc_t *gc, void *ptr, size_t size, 
  int flags, void(*dtor)(void*)) {

  gc->nitems++;
  gc->maxptr = ((uintptr_t)ptr) + size > gc->maxptr ? 
    ((uintptr_t)ptr) + size : gc->maxptr; 
  gc->minptr = ((uintptr_t)ptr)        < gc->minptr ? 
    ((uintptr_t)ptr)        : gc->minptr;

  if (tgc_resize_more(gc)) {
    tgc_add_ptr(gc, ptr, size, flags, dtor);
    if (!gc->paused && gc->nitems > gc->mitems) {
      tgc_run(gc);
    }
    return ptr;
  } else {
    gc->nitems--;
    free(ptr);
    return NULL;
  }
}

static void tgc_rem(tgc_t *gc, void *ptr) {
  tgc_rem_ptr(gc, ptr);
  tgc_resize_less(gc);
  gc->mitems = gc->nitems + gc->nitems / 2 + 1;
}

void *tgc_alloc(tgc_t *gc, size_t size) {
  return tgc_alloc_opt(gc, size, 0, NULL);
}

void *tgc_calloc(tgc_t *gc, size_t num, size_t size) {
  return tgc_calloc_opt(gc, num, size, 0, NULL);
}

void *tgc_realloc(tgc_t *gc, void *ptr, size_t size) {
  
  tgc_ptr_t *p;
  void *qtr = realloc(ptr, size);
  
  if (qtr == NULL) {
    tgc_rem(gc, ptr);
    return qtr;
  }

  if (ptr == NULL) {
    tgc_add(gc, qtr, size, 0, NULL);
    return qtr;
  }

  p  = tgc_get_ptr(gc, ptr);

  if (p && qtr == ptr) {
    p->size = size;
    return qtr;
  }

  if (p && qtr != ptr) {
    int flags = p->flags;
    void(*dtor)(void*) = p->dtor;
    tgc_rem(gc, ptr);
    tgc_add(gc, qtr, size, flags, dtor);
    return qtr;
  }

  return NULL;
}

void tgc_free(tgc_t *gc, void *ptr) {
  tgc_ptr_t *p  = tgc_get_ptr(gc, ptr);
  if (p) {
    if (p->dtor) {
      p->dtor(ptr);
    }
    free(ptr);
    tgc_rem(gc, ptr);
  }
}

void *tgc_alloc_opt(tgc_t *gc, size_t size, int flags, void(*dtor)(void*)) {
  void *ptr = malloc(size);
  if (ptr != NULL) {
    ptr = tgc_add(gc, ptr, size, flags, dtor);
  }
  return ptr;
}

void *tgc_calloc_opt(
  tgc_t *gc, size_t num, size_t size, 
  int flags, void(*dtor)(void*)) {
  void *ptr = calloc(num, size);
  if (ptr != NULL) {
    ptr = tgc_add(gc, ptr, num * size, flags, dtor);
  }
  return ptr;
}

void tgc_set_dtor(tgc_t *gc, void *ptr, void(*dtor)(void*)) {
  tgc_ptr_t *p  = tgc_get_ptr(gc, ptr);
  if (p) { p->dtor = dtor; }
}

void tgc_set_flags(tgc_t *gc, void *ptr, int flags) {
  tgc_ptr_t *p  = tgc_get_ptr(gc, ptr);
  if (p) { p->flags = flags; }
}

int tgc_get_flags(tgc_t *gc, void *ptr) {
  tgc_ptr_t *p  = tgc_get_ptr(gc, ptr);
  if (p) { return p->flags; }
  return 0;
}

void(*tgc_get_dtor(tgc_t *gc, void *ptr))(void*) {
  tgc_ptr_t *p  = tgc_get_ptr(gc, ptr);
  if (p) { return p->dtor; }
  return NULL;
}

size_t tgc_get_size(tgc_t *gc, void *ptr) {
  tgc_ptr_t *p  = tgc_get_ptr(gc, ptr);
  if (p) { return p->size; }
  return 0;
}


static tgc_t gc;

typedef struct dyn_array {
    union { // data types
        long* num;
        double* rnum;
        char** str;
        struct dyn_array** self;
    };
    
    int size;
} dyn_array;

dyn_array* create_array_long();
dyn_array* create_array_double();
dyn_array* create_array_string();
dyn_array* create_array_self();
int get_size(dyn_array* array);
long get_num(dyn_array* array, int index);
void add_num(dyn_array* array, long data);
void set_num(dyn_array* array, int index, long data);
void set_self(dyn_array* array, int index, dyn_array* data);
double get_rnum(dyn_array* array, int index);
dyn_array* get_self(dyn_array* array, int index);
void add_rnum(dyn_array* array, double data);
void set_rnum(dyn_array* array, int index, double data);
char* get_str(dyn_array* array, int index);
void add_str(dyn_array* array, char* data);
void set_str(dyn_array* array, int index, char* data);
void add_self(dyn_array* array, dyn_array* data);

dyn_array* create_array_long() {
    dyn_array* array = (dyn_array*)tgc_alloc(&gc, sizeof(dyn_array));

    if (array == (void*)0) {
        exit(1);
    }

    array->size = 0;
    array->num = (long*)tgc_alloc(&gc, sizeof(long));

    if (array->num == (void*)0) {
        tgc_free(&gc, array);
        exit(1);
    }

    return array;
}

dyn_array* create_array_double() {
    dyn_array* array = (dyn_array*)tgc_alloc(&gc, sizeof(dyn_array));

    if (array == (void*)0) {
        exit(1);
    }

    array->size = 0;
    array->rnum = (double*)tgc_alloc(&gc, sizeof(double));

    if (array->rnum == (void*)0) {
        tgc_free(&gc, array);
        exit(1);
    }

    return array;
}

dyn_array* create_array_string() {
    dyn_array* array = (dyn_array*)tgc_alloc(&gc,sizeof(dyn_array));

    if (array == (void*)0) {
        exit(1);
    }

    array->size = 0;
    array->str = (char**)tgc_alloc(&gc, sizeof(char*));

    if (array->str == (void*)0) {
        tgc_free(&gc, array);
        exit(1);
    }

    return array;
}

dyn_array* create_array_self() {
    dyn_array* array = (dyn_array*)tgc_alloc(&gc,sizeof(dyn_array));

    if (array == (void*)0) {
        exit(1);
    }

    array->size = 0;
    array->self = (dyn_array**)tgc_alloc(&gc, sizeof(dyn_array*));

    if (array->self == (void*)0) {
        tgc_free(&gc, array);
        exit(1);
    }

    return array;
}

int get_size(dyn_array* array) {
    return array->size;
}

long get_num(dyn_array* array, int index) {
    if (index < 0 || index >= array->size) {
        exit(1);
    }

    return array->num[index];
}

void add_num(dyn_array* array, long data) {
    if (array->num == (void*)0) {
        exit(1);
    }

    array->size++;
    array->num = (long*)tgc_realloc(&gc, array->num, array->size * sizeof(long));

    if (array->num == (void*)0) {
        exit(1);
    }

    array->num[array->size - 1] = data;
}

void set_num(dyn_array* array, int index, long data) {
    if (index < 0 || index >= array->size) {
        exit(1);
    }

    array->num[index] = data;
}

void set_self(dyn_array* array, int index, dyn_array* data) {
    if (index < 0 || index >= array->size) {
        exit(1);
    }

    array->self[index] = data;
}

double get_rnum(dyn_array* array, int index) {
    if (index < 0 || index >= array->size) {
        exit(1);
    }

    return array->rnum[index];
}

dyn_array* get_self(dyn_array* array, int index) {
    if (index < 0 || index >= array->size) {
        exit(1);
    }

    return array->self[index];
}

void add_rnum(dyn_array* array, double data) {
    if (array->rnum == (void*)0) {
        exit(1);
    }

    array->size++;
    array->rnum = (double*)tgc_realloc(&gc, array->rnum, array->size * sizeof(double));

    if (array->rnum == (void*)0) {
        exit(1);
    }

    array->rnum[array->size - 1] = data;
}

void set_rnum(dyn_array* array, int index, double data) {
    if (index < 0 || index >= array->size) {
        exit(1);
    }

    array->rnum[index] = data;
}

char* get_str(dyn_array* array, int index) {
    if (index < 0 || index >= array->size) {
        exit(1);
    }

    return array->str[index];
}

void add_str(dyn_array* array, char* data) {
    if (array->str == (void*)0) {
        exit(1);
    }

    array->size++;
    array->str = (char**)tgc_realloc(&gc, array->str, array->size * sizeof(char*));

    if (array->str == (void*)0) {
        exit(1);
    }

    array->str[array->size - 1] = data;
}

void set_str(dyn_array* array, int index, char* data) {
    if (index < 0 || index >= array->size) {
        exit(1);
    }

    array->str[index] = data;
}

void add_self(dyn_array* array, dyn_array* data) {
    if (array->self == (void*)0) {
        exit(1);
    }

    array->size++;
    array->self = (dyn_array**)tgc_realloc(&gc, array->self, array->size * sizeof(dyn_array*));

    if (array->self == (void*)0) {
        exit(1);
    }

    array->self[array->size - 1] = data;
}

#define push_back(vec, data) _Generic((data), \
    int: add_num, \
    long: add_num, \
    char*: add_str, \
    float: add_rnum, \
    double: add_rnum, \
    default: add_self \
)(vec, data)

#define at(vec, index, cast_type) _Generic((cast_type), \
    int: get_num, \
    long: get_num, \
    char*: get_str, \
    float: get_rnum, \
    double: get_rnum, \
    default: get_self \
)(vec, index)

#define set(vec, index, data) _Generic((data), \
    int: set_num, \
    long: set_num, \
    char*: set_str, \
    float: set_rnum, \
    double: set_rnum, \
    default: set_self \
)(vec, index, data)

#define num_t 0
#define rnum_t 0.1
#define str_t "0ab1a"
#define self_t (void*)0 // just a ptr

#define size(vec) ((vec)->size)

#define DYN_VECTOR_PUSH_BACK(vec, data) _Generic((data), \
    int: add_num, \
    long: add_num, \
    char*: add_str, \
    float: add_rnum, \
    double: add_rnum, \
    default: add_self \
)(vec, data)

#define DYN_VECTOR_AT(vec, index, cast_type) _Generic((cast_type), \
    int: get_num, \
    long: get_num, \
    char*: get_str, \
    float: get_rnum, \
    double: get_rnum, \
    default: get_self \
)(vec, index)

#define DYN_VECTOR_SET(vec, index, data) _Generic((data), \
    int: set_num, \
    long: set_num, \
    char*: set_str, \
    float: set_rnum, \
    double: set_rnum, \
    default: set_self \
)(vec, index, data)

#define TYPE_NUM 0
#define TYPE_RNUM 0.1
#define TYPE_STRING "0ab1a"
#define TYPE_SELF NULL // just a ptr

#define DYN_VECTOR_SIZE(vec) ((vec)->size)

#define print printf
#define malloc(x) tgc_alloc(&gc, x)
#define calloc(n, s) tgc_calloc(&gc, n, s)
#define realloc(p, s) tgc_realloc(&gc, p, s)
#define free(p) tgc_free(&gc, p)
    )";
    };
} // occultlang
