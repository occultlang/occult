#pragma once
#include "../parser/parser.hpp"

// TODO ARRAY DECLARATIONS FOR ARGUMENTS (dynamic)
// TODO match statement
// TODO i++ i-- etc
// TODO IMPORTS
// TODO FIX COMMENTS + REALNUMBERS
// TODO CASTING

// TESTING ALL CASES
// DECIDE MAYBE TUPLES
// TODO make arrays automatically delete (for now i dont really want to worry about this) 

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
        int array_var_count = 0;

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
        std::string generate(NodeType root, bool debug = false, int level = 2)
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
                        generated_source += "const char* ";
                    }

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
                            generated_source += "const char* ";
                        }

                        generated_source += args[i].second;

                        if (i != args.size() - 1)
                        {
                            generated_source += ", ";
                        }
                    }

                    generated_source += ") {\n";

                    if (debug)
                    {
                        std::cout << "body_start: " << check_type<occ_ast::body_start>(func_decl.second->get_child(idx)).first << std::endl;
                    }

                    for (int idx2 = idx; idx2 < func_decl.second->get_children().size(); idx2++)
                    {
                        generated_source += generate<occ_ast::body_start>(func_decl.second->get_child(idx2));
                    }

                    generated_source += "}\n";
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

                auto arr_decl = check_type<occ_ast::array_declaration>(node); // TODO IS STORE TYPE AND USE LATER FOR DYNAMIC TYPES
                if (arr_decl.first)
                {
                    if (auto n = check_type<occ_ast::num_declaration>(arr_decl.second->get_child()); n.first)
                    {
                        generated_source += "dyn_array* ";

                        auto id = check_type<occ_ast::identifier>(n.second->get_child()).second->content;

                        generated_source += id + " = create_array_long(0);\n";
                    }
                    else if (auto b = check_type<occ_ast::bool_declaration>(arr_decl.second->get_child()); b.first)
                    {
                        generated_source += "dyn_array* ";

                        auto id = check_type<occ_ast::identifier>(b.second->get_child()).second->content;

                        generated_source += id + " = create_array_long(0);\n";
                    }
                    else if (auto f = check_type<occ_ast::float_declaration>(arr_decl.second->get_child()); f.first)
                    {
                        generated_source += "dyn_array* ";

                        auto id = check_type<occ_ast::identifier>(f.second->get_child()).second->content;

                        generated_source += id + " = create_array_double(0);\n";
                    }
                    else if (auto s = check_type<occ_ast::string_declaration>(arr_decl.second->get_child()); s.first)
                    {
                        generated_source += "dyn_array* ";

                        auto id = check_type<occ_ast::identifier>(s.second->get_child()).second->content;

                        generated_source += id + " = create_array_string(0);\n";
                    }
                }

                auto string_literal = check_type<occ_ast::string_literal>(node);
                if (string_literal.first)
                {
                    if (debug)
                        std::cout << "string_literal: " << string_literal.first << std::endl;
                    if (auto a1 = check_type<occ_ast::string_literal>(node); a1.first)
                    {
                        generated_source += a1.second->content;
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
                    generated_source += "const char* ";
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
            }

            return generated_source;
        }

            std::string lib = R"(
enum {
    dyn_long,
    dyn_float,
    dyn_str
} dyn_type;

typedef struct dyn_array {
    union { // data types
        long* num;
        double* rnum;
        const char** str;
    };
    
    int size;
    int type;
} dyn_array;

dyn_array* create_array_long(int size) {
    dyn_array* array = (dyn_array*)malloc(sizeof(dyn_array));
    array->type = dyn_long;

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
    array->type = dyn_float;

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
    array->type = dyn_str;

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

int get_array_size(dyn_array* array) {
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
    array->num = (long*)realloc(array->num, array->size * sizeof(long));

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

double get_rnum(dyn_array* array, int index) {
    if (index < 0 || index >= array->size) {
        exit(1);
    }

    return array->rnum[index];
}

void add_rnum(dyn_array* array, double data) {
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

void set_rnum(dyn_array* array, int index, double data) {
    if (index < 0 || index >= array->size) {
        exit(1);
    }

    array->rnum[index] = data;
}

const char* get_str(dyn_array* array, int index) {
    if (index < 0 || index >= array->size) {
        exit(1);
    }

    return array->str[index];
}

void add_str(dyn_array* array, const char* data) {
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

void set_str(dyn_array* array, int index, const char* data) {
    if (index < 0 || index >= array->size) {
        exit(1);
    }

    array->str[index] = data;
}

void delete(dyn_array* array) {
    switch (array->type) {
        case dyn_long:
            free(array->num);
            break;
        case dyn_float:
            free(array->rnum);
            break;
        case dyn_str:
            free(array->str);
            break;
    }
    free(array);
}
    )";
    };
} // occultlang
