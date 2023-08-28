#include "code_generator.hpp"

namespace occultlang {
    std::string code_generator::generate() {
        std::string source;

        for (auto& child : root->get_children()) {
            if (auto func_decl = std::dynamic_pointer_cast<ast_function_declaration>(child)) {
                if (func_decl->type == "i32") {
                    source = source + "std::int32_t" + " " + func_decl->name + "(";
                }

                source += ") { }\n";
            }
            else {
                // Handle unknown node type or raise an error
            }
        }

        return source;
    }
} // occultlang