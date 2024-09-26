#pragma once
#include <vector>
#include <memory>
#include "lexer.hpp"

namespace occult {
  enum class ast_type {
    root,
    binaryexpr,
    literal,
    block,
    identifier,
    function,
    ifstmt,
    elsestmt,
    elseifstmt,
    loopstmt,
    whilestmt,
    forstmt,
    continuestmt,
    breakstmt,
    returnstmt,
    functionarguments,
    datatype,
    assignment,
  };
  
  class ast {
    std::vector<std::unique_ptr<ast>> children;
  public:
    ast() = default;
    
    std::string content = ""; // base class
    
    template<typename BaseAst = ast>
    static std::unique_ptr<BaseAst> new_node() {
      return std::make_unique<BaseAst>();
    }
    
    void add_child(std::unique_ptr<ast> child) {
        children.push_back(std::move(child));
    }
    
    std::vector<std::unique_ptr<ast>>& get_children() {
        return children;
    }
    
    virtual ast_type get_type() = 0;
    
    virtual std::string to_string() = 0;
    
    void visualize(int depth = 0) {
        std::string indent(depth * 2, ' ');
        std::print("{}", indent + to_string());
        
        if (!content.empty())
          std::print(": {}\n", content);
        else
          std::println();
        
        for (const auto& child : children) {
            child->visualize(depth + 1);
        }
    }
  };
  
  #define NODE(tt, nn)  \
  class nn : public ast { \
    public: \
    ast_type get_type() override { \
      return ast_type::tt; \
    } \
    std::string to_string() override { \
      return #nn; \
    } \
  };
  
  NODE(root, ast_root)
  NODE(binaryexpr, ast_binaryexpr)
  NODE(literal, ast_literal)
  NODE(datatype, ast_datatype)
  NODE(block, ast_block)
  NODE(identifier, ast_identifier)
  NODE(function, ast_function)
  NODE(functionarguments, ast_functionargs)
  NODE(ifstmt, ast_ifstmt)
  NODE(elsestmt, ast_elsestmt)
  NODE(elseifstmt, ast_elseifstmt)
  NODE(loopstmt, ast_loopstmt)
  NODE(whilestmt, ast_whilestmt)
  NODE(forstmt, ast_forstmt)
  NODE(continuestmt, ast_continuestmt)
  NODE(breakstmt, ast_breakstmt)
  NODE(returnstmt, ast_returnstmt)
  //NODE(instmt, ast_instmt)
  NODE(assignment, ast_assignment)
} // namespace occult
