#include "ir_gen.hpp"

namespace occult {
  void ir_generator::emit_block(std::unique_ptr<ast_block> block) {
    
    block.release();
  }  
  
  void ir_generator::emit_function(std::unique_ptr<ast_function> function) {
    for (const auto& c : function->get_children()) {
      switch(c->get_type()) {
        case ast_type::identifier: {
          
          break;
        }
        default: {
          break;
        }
      }
    }
    
    function.release();
  }
  
  const std::string& ir_generator::generate() {
    for (const auto& c : root->get_children()) {
      switch(c->get_type()) {
        case ast_type::function: {
          auto func_node = ast::cast<ast_function>(c.get());
          emit_function(std::move(func_node));
          break;
        }
        default: {
          throw std::runtime_error("language doesn't support whatever you put in, that ain't supposed to be there");
        }
      }
    }
    
    return ir_source;
  }
} // namespace occult
