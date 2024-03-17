#pragma once
#include "../parser/parser.hpp"

namespace occultlang {
    enum debug_level {
        all = 0,
        some = 1,
        none = 2
    };

    class code_gen {
    public:
        ~code_gen() = default;

        template<typename CheckType, typename NodeType>
        std::pair<bool, std::shared_ptr<CheckType>> check_type(NodeType node, bool debug = false, int level = 2) {
            if (auto t = std::dynamic_pointer_cast<CheckType>(node); t != nullptr) {
                if (debug && level == 0) {
                    std::cout << "type: " << typeid(t).name() << std::endl;
                }
                return std::make_pair(true, t);
            }

            return std::make_pair(false, nullptr);
        }

        template<typename AstType, typename NodeType = std::shared_ptr<AstType>>
        std::string compile(NodeType root, bool debug = false, int level = 2) {
            std::string generated_source;

            for (int i = 0; i < root->get_children().size(); i++) {
                auto node = root->get_child(i);

                if (node == nullptr) {
                    node = root;
                }
                
                auto func_decl = check_type<occ_ast::function_declaration>(node); 
                if (func_decl.first){
                    int idx = 0;

                    auto func_name = check_type<occ_ast::identifier>(func_decl.second->get_child(idx++)).second->content;

                    if (debug && level == 0) {
                        std::cout << "idx value: " << idx << std::endl;
                    }

                    if (debug && (level == 1 || level == 0)) {
                        std::cout << "function_name: " << func_name << std::endl;
                    }

                    auto next = func_decl.second->get_child(idx); // type or arguments

                    std::vector<std::pair<std::string, std::string>> args;  

                    auto next_typename = std::string();
                    while (func_decl.second->get_child(idx)->has_child()) {
                        next = func_decl.second->get_child(idx++); 
                        
                        if (auto n1 = check_type<occ_ast::num_declaration>(next); n1.first) {
                            next_typename = n1.second->to_string();
                        } 
                        else if (auto n2 = check_type<occ_ast::bool_declaration>(next); n2.first) {
                            next_typename = n2.second->to_string();
                        }
                        else if (auto n3 = check_type<occ_ast::float_declaration>(next); n3.first) {
                            next_typename = n3.second->to_string();
                        }
                        else if (auto n4 = check_type<occ_ast::string_declaration>(next); n4.first) {
                            next_typename = n4.second->to_string();
                        }

                        auto args_name = check_type<occ_ast::identifier>(next->get_child()).second->content;

                        if (debug && level == 0) {
                            std::cout << "idx value: " << idx << std::endl;
                        }

                        if (debug && (level == 1 || level == 0)) {
                            std::cout << "args name: " << args_name << std::endl;
                            std::cout << "args type: " << next_typename << std::endl;
                        }

                        args.push_back(std::make_pair(next_typename, args_name)); // add to vector
                    }

                    if (debug && (level == 1 || level == 0)) {
                        std::cout << "args size: " << args.size() << std::endl;
                    }

                    next = func_decl.second->get_child(idx++); // func type

                    if (debug && level == 0) {
                        std::cout << "idx value: " << idx << std::endl;
                    }

                    if (auto n1 = check_type<occ_ast::num_declaration>(next); n1.first) {
                        next_typename = n1.second->to_string();
                    } 
                    else if (auto n1 = check_type<occ_ast::float_declaration>(next); n1.first) {
                        next_typename = n1.second->to_string();
                    } 
                    else if (auto n2 = check_type<occ_ast::bool_declaration>(next); n2.first) {
                        next_typename = n2.second->to_string();
                    }
                    else if (auto n3 = check_type<occ_ast::void_declaration>(next); n3.first) {
                        next_typename = n3.second->to_string();
                    }
                    else if (auto n4 = check_type<occ_ast::string_declaration>(next); n4.first) {
                        next_typename = n4.second->to_string();
                    }

                    if (debug && (level == 1 || level == 0)) {
                        std::cout << "function_type: " << next_typename << std::endl;
                    }

                    // generate code

                    if (next_typename == "void_declaration") {
                        generated_source += "void ";
                    } 
                    else if (next_typename == "num_declaration") {
                        generated_source += "int ";
                    } 
                    else if (next_typename == "float_declaration") {
                        generated_source += "double ";
                    }
                    else if (next_typename == "bool_declaration") {
                        generated_source += "bool ";
                    } 
                    else if (next_typename == "string_declaration") {
                        generated_source += "const char* ";
                    }

                    if (func_name == "main") {
                        generated_source += "_start(";
                    }
                    else {
                        generated_source += func_name + "(";
                    }

                    for (int i = 0; i < args.size(); i++) {
                        if (args[i].first == "num_declaration") {
                            generated_source += "int ";
                        } 
                        else if (args[i].first == "float_declaration") {
                            generated_source += "double ";
                        }
                        else if (args[i].first == "bool_declaration") {
                            generated_source += "bool ";
                        } 
                        else if (args[i].first == "string_declaration") {
                            generated_source += "const char* ";
                        }

                        generated_source += args[i].second;

                        if (i != args.size() - 1) {
                            generated_source += ", ";
                        }
                    }

                    generated_source += ") {\n";

                    if (func_decl.second->get_child(idx)->is_last()) { // do checks to make body_start be checked (we have issues with multiple variables)
                        std::cout << "body_start: " << check_type<occ_ast::body_start>(func_decl.second->get_child(idx)).first << std::endl;
                        generated_source += compile<occ_ast::body_start>(func_decl.second->get_child(idx));
                        generated_source += "}";
                    }
                    else {
                        generated_source += "}";
                    }
                }

                auto return_decl = check_type<occ_ast::return_declaration>(node);
                if (return_decl.first) {
                    std::cout << "return_decl: " << return_decl.first << std::endl;

                    std::cout << "return_decl children: " << return_decl.second->get_children().size() << std::endl;

                    generated_source += "return ";

                    if (return_decl.second->get_child(0)->is_last()) {
                        if (check_type<occ_ast::number_literal>(return_decl.second->get_child(0)).first) {
                            std::cout << "num literal: " << check_type<occ_ast::number_literal>(return_decl.second->get_child(0)).first << std::endl;
                            generated_source += compile<occ_ast::number_literal>(return_decl.second);
                        }
                    }

                    generated_source += ";\n";
                }

                auto num_literal = check_type<occ_ast::number_literal>(node); 
                if (num_literal.first) {
                    std::cout << "num literal: " << num_literal.first << std::endl;
                    if (auto a1 = check_type<occ_ast::number_literal>(node); a1.first) {
                        generated_source += a1.second->content;
                    }
                }

                auto num_decl = check_type<occ_ast::num_declaration>(node);
                if (num_decl.first) {
                    std::cout << "num_decl: " << num_decl.first << std::endl;
                    generated_source += "int ";

                    std::cout << "num_decl children: " << num_decl.second->get_children().size() << std::endl;

                    auto num_name = num_decl.second->get_child(0);

                    if (auto n1 = check_type<occ_ast::identifier>(num_name); n1.first) {
                        std::cout << "num_name: " << n1.first << std::endl;

                        auto num_name_content = n1.second->content;

                        generated_source += num_name_content;

                        if (auto assignment = check_type<occ_ast::assignment>(num_decl.second->get_child(1)); assignment.first) { // maybe add other assignments *= /= etc
                            std::cout << "assignment: " << assignment.first << std::endl;

                            generated_source += " = ";

                            for (int j = 0; j < assignment.second->get_children().size(); j++) { // add other types (operators, delimiters, etc)
                                auto child_assignment = assignment.second->get_child(j);

                                if (auto a1 = check_type<occ_ast::number_literal>(child_assignment); a1.first) {
                                    generated_source += a1.second->content;
                                }
                                else if (auto a2 = check_type<occ_ast::identifier>(child_assignment); a2.first) {
                                    generated_source += a2.second->content;
                                }
                                else if (auto a3 = check_type<occ_ast::operator_declaration>(child_assignment); a3.first) {
                                    generated_source += a3.second->content;
                                }
                                else if (auto a4 = check_type<occ_ast::delimiter_declaration>(child_assignment); a4.first) {
                                    generated_source += a4.second->content;
                                }
                            }
                        }
                    }
                    else {
                        std::cout << "num_name: " << n1.first << std::endl;
                    }

                    generated_source += ";\n";
                }
            }

            return generated_source;
        }
    };
} // occultlang
