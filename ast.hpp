#pragma once
#include <vector>
#include <memory>
#include "lexer.hpp"

namespace occult {
  enum class ast_type {
    root,
    binaryexpr,
    literalexpr,
    block,
    identifier,
    function,
    ifstmt,
    elsestmt,
    elseifstmt,
    loopstmt,
    whilestmt,
    forstmt,
    matchstmt,
    caseblock,
    defaultcase,
    continuestmt,
    breakstmt,
    returnstmt,
    instmt,
  };
  
  class ast {
    std::vector<std::unique_ptr<ast>> children;
  public:
    ast() = default;
    
    std::string content = "base"; // base class
    
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
        std::println("{}", indent + to_string());
        
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
  NODE(literalexpr, ast_literalexpr)
  NODE(block, ast_block)
  NODE(identifier, ast_identifier)
  NODE(function, ast_function)
  NODE(ifstmt, ast_ifstmt)
  NODE(elsestmt, ast_elsestmt)
  NODE(elseifstmt, ast_elseifstmt)
  NODE(loopstmt, ast_loopstmt)
  NODE(whilestmt, ast_whilestmt)
  NODE(forstmt, ast_forstmt)
  NODE(matchstmt, ast_matchstmt)
  NODE(caseblock, ast_caseblock)
  NODE(defaultcase, ast_defaultcase)
  NODE(continuestmt, ast_continuestmt)
  NODE(breakstmt, ast_breakstmt)
  NODE(returnstmt, ast_returnstmt)
  NODE(instmt, ast_instmt)
}