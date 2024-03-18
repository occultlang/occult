#pragma once
#include <vector>
#include <string>
#include <memory>
#include <cstdint>
#include <limits>
#include <cmath>
#include "../data_structures/tree.hpp"

namespace occultlang
{ // for expressions, operator precedence doesn't matter, we're transpiling, a C++ compiler will take care of it :)
	enum ast_type
	{ /* there is one type for integers and floats :) */
	  root,
	  num_declaration,
	  bool_declaration,
	  function_declaration,
	  loop_declaration,
	  if_declaration,
	  return_declaration,
	  assignment,
	  identifier,
	  number_literal,
	  string_literal,
	  float_literal,
	  boolean_literal,
	  function_call,
	  string_declaration,
	  void_declaration,
	  break_declaration,
	  continue_declaration,
	  while_declaration,
	  for_declaration,
	  float_declaration,
	  comma,
	  delimiter_declaration,
	  operator_declaration,
	  postfix_or_prefix,
	  body_start,
	  body_end,
	  elseif_declaration,
	  else_declaration
	};

	struct ast : public tree<std::shared_ptr<ast>>
	{
		ast() : tree<std::shared_ptr<ast>>(nullptr) {}

		ast(std::string content) : tree<std::shared_ptr<ast>>(nullptr)
		{
			this->content = content;
		}

		virtual ~ast() {}

		virtual ast_type get_type() { return ast_type::root; }

		virtual std::string to_string() { return "root"; }

		static void visualize(std::shared_ptr<ast> tree, int indent = 0);

		std::string content;
	};

	namespace occ_ast
	{
		/*
			example:

			number x = 0;

			ast:

			num_declaration:
				assignment:
					identifier: x
					number_literal: 0
		*/

		struct num_declaration : public ast
		{
			num_declaration() : ast() {}
			virtual ~num_declaration() {}
			num_declaration(std::string content) {}

			virtual ast_type get_type() { return ast_type::num_declaration; }

			virtual std::string to_string() { return "num_declaration"; }
		};

		struct float_declaration : public ast
		{
			float_declaration() : ast() {}
			virtual ~float_declaration() {}
			float_declaration(std::string content) {}

			virtual ast_type get_type() { return ast_type::float_declaration; }

			virtual std::string to_string() { return "float_declaration"; }
		};

		/*
			example:

			bool x = true;

			ast:

			bool_declaration:
				assignment:
					identifier: x
					boolean_literal: true
		*/

		struct bool_declaration : public ast
		{
			bool_declaration() : ast() {}
			virtual ~bool_declaration() {}
			bool_declaration(std::string content) {}

			virtual ast_type get_type() { return ast_type::bool_declaration; }

			virtual std::string to_string() { return "bool_declaration"; }
		};

		struct string_declaration : public ast
		{
			string_declaration() : ast() {}
			virtual ~string_declaration() {}
			string_declaration(std::string content) {}

			virtual ast_type get_type() { return ast_type::string_declaration; }

			virtual std::string to_string() { return "string_declaration"; }
		};

		struct void_declaration : public ast
		{
			void_declaration() : ast() {}
			virtual ~void_declaration() {}
			void_declaration(std::string content) {}

			virtual ast_type get_type() { return ast_type::void_declaration; }

			virtual std::string to_string() { return "void_declaration"; }
		};

		/*
			example:

			fn main(number x) number {  }

			ast:

			function_declaration:

				identifier: main 	# function name

				num_declaration		# argument
					identifier: x

				num_declaration		# return type

				# rest is the body of the function, so "return 0"

				return_declaration:
					number_literal: 0
		*/

		struct function_declaration : public ast
		{
			function_declaration() : ast() {}
			virtual ~function_declaration() {}
			function_declaration(std::string content) {}

			virtual ast_type get_type() { return ast_type::function_declaration; }

			virtual std::string to_string() { return "function_declaration"; }
		};

		/*
			example:

			loop x == 0 {
				number i = 0;
			}

			ast:

			loop_declaration:
				equal:				# condition
					identifier: x
					number_literal: 0

				# rest is body

				num_declaration:
					assignment:
						identifier: i
						number_literal: 0
		*/

		struct loop_declaration : public ast
		{
			loop_declaration() : ast() {}
			virtual ~loop_declaration() {}
			loop_declaration(std::string content) {}

			virtual ast_type get_type() { return ast_type::loop_declaration; }

			virtual std::string to_string() { return "loop_declaration"; }
		};

		struct break_declaration : public ast
		{ // ast is just one break statement lol
			break_declaration() : ast() {}
			virtual ~break_declaration() {}
			break_declaration(std::string content) {}

			virtual ast_type get_type() { return ast_type::break_declaration; }

			virtual std::string to_string() { return "break_declaration"; }
		};

		/*
			example:

			if x == 0 {
				number i = 0;
			}

			ast:

			if_declaration:
				equal:				# condition
					identifier: x
					number_literal: 0

				# rest is body

				num_declaration:
					assignment:
						identifier: i
						number_literal: 0
		*/

		struct if_declaration : public ast
		{
			if_declaration() : ast() {}
			virtual ~if_declaration() {}
			if_declaration(std::string content) {}

			virtual ast_type get_type() { return ast_type::if_declaration; }

			virtual std::string to_string() { return "if_declaration"; }
		};

		struct elseif_declaration : public ast
		{
			elseif_declaration() : ast() {}
			virtual ~elseif_declaration() {}
			elseif_declaration(std::string content) {}

			virtual ast_type get_type() { return ast_type::elseif_declaration; }

			virtual std::string to_string() { return "elseif_declaration"; }
		};

		struct else_declaration : public ast
		{
			else_declaration() : ast() {}
			virtual ~else_declaration() {}
			else_declaration(std::string content) {}

			virtual ast_type get_type() { return ast_type::else_declaration; }

			virtual std::string to_string() { return "else_declaration"; }
		};

		/*
			example:

			return 0;

			ast:

			return_declaration:
				number_literal: 0
		*/

		struct return_declaration : public ast
		{
			return_declaration() : ast() {}
			virtual ~return_declaration() {}
			return_declaration(std::string content) {}

			virtual ast_type get_type() { return ast_type::return_declaration; }

			virtual std::string to_string() { return "return_declaration"; }
		};

		struct assignment : public ast
		{
			assignment() : ast() {}
			virtual ~assignment() {}
			assignment(std::string content) {}

			virtual ast_type get_type() { return ast_type::assignment; }

			virtual std::string to_string() { return "assignment"; }
		};

		struct identifier : public ast
		{
			identifier() : ast() {}
			virtual ~identifier() {}
			identifier(std::string content) {}

			virtual ast_type get_type() { return ast_type::identifier; }

			virtual std::string to_string() { return "identifier: " + content; }
		};

		struct number_literal : public ast
		{
			number_literal() : ast() {}
			virtual ~number_literal() {}
			number_literal(std::string content) {}

			virtual ast_type get_type() { return ast_type::number_literal; }

			virtual std::string to_string() { return "number_literal: " + content; }
		};

		struct string_literal : public ast
		{
			string_literal() : ast() {}
			virtual ~string_literal() {}
			string_literal(std::string content) {}

			virtual ast_type get_type() { return ast_type::string_literal; }

			virtual std::string to_string() { return "string_literal: " + content; }
		};

		struct float_literal : public ast
		{
			float_literal() : ast() {}
			virtual ~float_literal() {}
			float_literal(std::string content) {}

			virtual ast_type get_type() { return ast_type::float_literal; }

			virtual std::string to_string() { return "float_literal: " + content; }
		};

		struct boolean_literal : public ast
		{
			boolean_literal() : ast() {}
			virtual ~boolean_literal() {}
			boolean_literal(std::string content) {}

			virtual ast_type get_type() { return ast_type::boolean_literal; }

			virtual std::string to_string() { return "boolean_literal: " + content; }
		};

		/* function_call example
			example:

			fn main() number {
				return add(1, 2);
			}

			ast:

			function_declaration:
				identifier: main 	# function name

				num_declaration		# return type

				# rest is the body of the function, so "return add(1, 2)"

				return_declaration:
					function_call:
						identifier: add # name
						number_literal: 1 # arguments
						number_literal: 2
		*/

		struct function_call : public ast
		{
			function_call() : ast() {}
			virtual ~function_call() {}
			function_call(std::string content) {}

			virtual ast_type get_type() { return ast_type::function_call; }

			virtual std::string to_string() { return "function_call"; }
		};

		struct operator_declaration : public ast
		{
			operator_declaration() : ast() {}
			virtual ~operator_declaration() {}
			operator_declaration(std::string content) {}

			virtual ast_type get_type() { return ast_type::operator_declaration; }

			virtual std::string to_string() { return "operator_declaration: " + content; }
		};

		struct delimiter_declaration : public ast
		{
			delimiter_declaration() : ast() {}
			virtual ~delimiter_declaration() {}
			delimiter_declaration(std::string content) {}

			virtual ast_type get_type() { return ast_type::delimiter_declaration; }

			virtual std::string to_string() { return "delimiter_declaration: " + content; }
		};

		struct continue_declaration : public ast
		{
			continue_declaration() : ast() {}
			virtual ~continue_declaration() {}
			continue_declaration(std::string content) {}

			virtual ast_type get_type() { return ast_type::continue_declaration; }

			virtual std::string to_string() { return "continue_declaration"; }
		};

		struct while_declaration : public ast
		{
			while_declaration() : ast() {}
			virtual ~while_declaration() {}
			while_declaration(std::string content) {}

			virtual ast_type get_type() { return ast_type::while_declaration; }

			virtual std::string to_string() { return "while_declaration"; }
		};

		struct for_declaration : public ast
		{
			for_declaration() : ast() {}
			virtual ~for_declaration() {}
			for_declaration(std::string content) {}

			virtual ast_type get_type() { return ast_type::for_declaration; }

			virtual std::string to_string() { return "for_declaration"; }
		};

		struct postfix_or_prefix : public ast
		{
			postfix_or_prefix() : ast() {}
			virtual ~postfix_or_prefix() {}
			postfix_or_prefix(std::string content) {}

			virtual ast_type get_type() { return ast_type::postfix_or_prefix; }

			virtual std::string to_string() { return "postfix_or_prefix"; }
		};

		struct body_start : public ast
		{
			body_start() : ast() {}
			virtual ~body_start() {}
			body_start(std::string content) {}

			virtual ast_type get_type() { return ast_type::body_start; }

			virtual std::string to_string() { return "body_start"; }
		};

		struct body_end : public ast
		{
			body_end() : ast() {}
			virtual ~body_end() {}
			body_end(std::string content) {}

			virtual ast_type get_type() { return ast_type::body_end; }

			virtual std::string to_string() { return "body_end"; }
		};

		struct comma : public ast 
		{
			comma() : ast() {}
			virtual ~comma() {}
			comma(std::string content) {}

			virtual ast_type get_type() { return ast_type::comma; }

			virtual std::string to_string() { return "comma"; }
		};
	}
} // occultlang
