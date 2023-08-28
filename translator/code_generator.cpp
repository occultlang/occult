#include "code_generator.hpp"

namespace occultlang {
	std::string code_generator::generate(std::shared_ptr<ast> node) {
		std::string generated_code;

		for (int i = 0; i < node->get_children().size(); i++) {
			auto child_node = node->get_children()[i];

			if (auto func_decl = std::dynamic_pointer_cast<ast_function_declaration>(child_node)) {
				if (func_decl->type == "i8") {
					generated_code += "std::int8_t " + func_decl->name + "(";
				}
				else if (func_decl->type == "i8_pointer") {
					generated_code += "std::int8_t* " + func_decl->name + "(";
				}
				else if (func_decl->type == "i16") {
					generated_code += "std::int16_t " + func_decl->name + "(";
				}
				else if (func_decl->type == "i16_pointer") {
					generated_code += "std::int16_t* " + func_decl->name + "(";
				}
				else if (func_decl->type == "i32") {
					generated_code += "std::int32_t " + func_decl->name + "(";
				}
				else if (func_decl->type == "i32_pointer") {
					generated_code += "std::int32_t* " + func_decl->name + "(";
				}
				else if (func_decl->type == "i64") {
					generated_code += "std::int64_t " + func_decl->name + "(";
				}
				else if (func_decl->type == "i64_pointer") {
					generated_code += "std::int64_t* " + func_decl->name + "(";
				}

				auto args_node = child_node->get_children()[0];  // assuming arguments node is the first child
				if (auto args = std::dynamic_pointer_cast<ast_arguments>(args_node)) {
					for (int j = 0; j < args->get_children().size(); j++) {
						auto arg_node = args->get_children()[j];
						if (auto var_decl = std::dynamic_pointer_cast<ast_variable_declaration>(arg_node)) {

							if (var_decl->type == "i8") {
								generated_code += "std::int8_t " + var_decl->name;
							}
							else if (var_decl->type == "i8_pointer") {
								generated_code += "std::int8_t* " + var_decl->name;
							}
							else if (var_decl->type == "i16") {
								generated_code += "std::int16_t " + var_decl->name;
							}
							else if (var_decl->type == "i16_pointer") {
								generated_code += "std::int16_t* " + var_decl->name;
							}
							else if (var_decl->type == "i32") {
								generated_code += "std::int32_t " + var_decl->name;
							}
							else if (var_decl->type == "i32_pointer") {
								generated_code += "std::int32_t* " + var_decl->name;
							}
							else if (var_decl->type == "i64") {
								generated_code += "std::int64_t " + var_decl->name;
							}
							else if (var_decl->type == "i64_pointer") {
								generated_code += "std::int64_t* " + var_decl->name;
							}

							if (j < args->get_children().size() - 1) {
								generated_code += ", ";
							}
						}
					}
				}

				generated_code += ") {\n";  // start of function body

				generated_code += generate(std::dynamic_pointer_cast<ast>(child_node->get_children()[1]));

				generated_code += "}\n";  // end of function body
			}
			else if (auto variable_decl = std::dynamic_pointer_cast<ast_variable_declaration>(child_node)) {
				if (variable_decl->type == "i8") {
					generated_code += "std::int8_t " + variable_decl->name + " = ";
				}
				else if (variable_decl->type == "i8_pointer") {
					generated_code += "std::int8_t* " + variable_decl->name + " = ";
				}
				else if (variable_decl->type == "i16") {
					generated_code += "std::int16_t " + variable_decl->name + " = ";
				}
				else if (variable_decl->type == "i16_pointer") {
					generated_code += "std::int16_t* " + variable_decl->name + " = ";
				}
				else if (variable_decl->type == "i32") {
					generated_code += "std::int32_t " + variable_decl->name + " = ";
				}
				else if (variable_decl->type == "i32_pointer") {
					generated_code += "std::int32_t* " + variable_decl->name + " = ";
				}
				else if (variable_decl->type == "i64") {
					generated_code += "std::int65_t " + variable_decl->name + " = ";
				}
				else if (variable_decl->type == "i64_pointer") {
					generated_code += "std::int64_t* " + variable_decl->name + " = ";
				}

				generated_code += generate(std::dynamic_pointer_cast<ast>(child_node));

				generated_code += ";\n";  // end of variable declaration
			}
			else if (auto number_decl = std::dynamic_pointer_cast<ast_number_literal>(child_node)) {
				generated_code += number_decl->value;
			}
			else if (auto return_decl = std::dynamic_pointer_cast<ast_return_statement>(child_node)) {
				generated_code += "return ";

				generated_code += generate(std::dynamic_pointer_cast<ast>(child_node));

				generated_code += ";\n";
			}
			else if (auto function_call_decl = std::dynamic_pointer_cast<ast_function_call>(child_node)) { // fix this, its broken
				if (function_call_decl->name == "print") {
					generated_code += "std::cout << ";

					auto args_node = child_node->get_children()[0];  // assuming arguments node is the first child
					if (auto args = std::dynamic_pointer_cast<ast_arguments>(args_node)) {
						for (int j = 0; j < args->get_children().size(); j++) {
							auto arg_node = args->get_children()[j];

							generated_code += generate(std::dynamic_pointer_cast<ast>(args_node));
						}
					}

					generated_code += " << std::endl;\n";
				}

				// do other function calls
			}
		}

		return generated_code;
	}

	std::string code_generator::generate() {
		return generate(root);
	}
} // occultlang