#include "parser.hpp"
#include "../lexer/lexer.hpp"
#include <string>
#include <iostream>
#include <exception>
#include <fstream>
#include <sstream>

namespace occultlang
{
	token parser::consume(token_type tt)
	{
		if (position < tokens.size() && tokens[position].get_type() == tt)
		{
			return tokens[position++];
		}
	}

	token parser::consume(token_type tt, token_type tt2)
	{
		if (position < tokens.size() && (tokens[position].get_type() == tt || tokens[position].get_type() == tt2))
		{
			return tokens[position++];
		}
	}

	token parser::peek()
	{
		return tokens[position];
	}

	token parser::peek(int amount)
	{
		return tokens[position + amount];
	}

	token parser::peek_next()
	{
		return tokens[position + 1];
	}

	token parser::consume()
	{
		if (position < tokens.size())
		{
			return tokens[position++];
		}

		throw occ_runtime_error("Expected token", peek());
	}

	bool parser::match(token_type tt, std::string match)
	{
		return (tokens[position].get_type() == tt && tokens[position].get_lexeme() == match) ? true : false;
	}

	bool parser::match(token_type tt)
	{
		return (tokens[position].get_type() == tt) ? true : false;
	}

	bool parser::match_next(token_type tt, std::string match)
	{
		return (tokens[position + 1].get_type() == tt && tokens[position + 1].get_lexeme() == match) ? true : false;
	}

	bool parser::match_next(token_type tt)
	{
		return (tokens[position + 1].get_type() == tt) ? true : false;
	}

	std::shared_ptr<ast> parser::parse_identifier()
	{
		if (match(tk_identifier))
		{
			std::shared_ptr<ast> identifier = std::make_shared<occ_ast::identifier>();

			identifier->content = consume(tk_identifier).get_lexeme();

			return identifier;
		}
		else
		{
			throw occ_runtime_error(parse_exceptions[EXPECTED_IDENTIFIER], peek());
		}
	}

	std::shared_ptr<ast> parser::parse_assignment()
	{
		if (match(tk_operator, "="))
		{
			consume(tk_operator);

			std::shared_ptr<ast> assignment = std::make_shared<occ_ast::assignment>();

			if (match(tk_identifier) && match_next(tk_delimiter, "("))
			{
				auto function_call = std::make_shared<occ_ast::function_call>();

				auto expression = parse_expression();
				for (auto &child : expression->get_children())
				{
					child->swap_parent(function_call);
				}

				assignment->add_child(function_call);
			}
			else
			{
				auto expression = parse_expression();
				for (auto &child : expression->get_children())
				{
					child->swap_parent(assignment);
				}
			}

			return assignment;
		}
		else
		{
			throw occ_runtime_error(parse_exceptions[EXPECTED_EQUAL], peek());
		}
	}

	std::shared_ptr<ast> parser::parse_if()
	{
		if (match(tk_keyword, "if"))
		{
			consume(tk_keyword);

			std::shared_ptr<ast> if_declaration = std::make_shared<occ_ast::if_declaration>();

			auto expression = parse_expression();
			for (auto &child : expression->get_children())
			{
				child->swap_parent(if_declaration);
			}

			if (match(tk_delimiter, "{"))
			{
				consume(tk_delimiter);

				while (!match(tk_delimiter, "}"))
				{
					auto statement = std::make_shared<occ_ast::body_start>();

					statement->add_child(parse_keywords()); // body

					if (match(tk_delimiter, ";"))
					{
						consume(tk_delimiter);
					}

					if (statement)
					{
						if_declaration->add_child(statement);
					}

					statement->add_child(std::make_shared<occ_ast::body_end>());
				}

				consume(tk_delimiter);
			}
			else
			{
				throw occ_runtime_error(parse_exceptions[EXPECTED_LEFT_BRACE], peek());
			}

			return if_declaration;
		}
	}

	std::shared_ptr<ast> parser::parse_elseif()
	{
		if (match(tk_keyword, "else") && match_next(tk_keyword, "if"))
		{
			consume(tk_keyword);
			consume(tk_keyword);

			std::shared_ptr<ast> elseif_declaration = std::make_shared<occ_ast::elseif_declaration>();

			auto expression = parse_expression();
			for (auto &child : expression->get_children())
			{
				child->swap_parent(elseif_declaration);
			}

			if (match(tk_delimiter, "{"))
			{
				consume(tk_delimiter);

				while (!match(tk_delimiter, "}"))
				{
					auto statement = std::make_shared<occ_ast::body_start>();

					statement->add_child(parse_keywords()); // body

					if (match(tk_delimiter, ";"))
					{
						consume(tk_delimiter);
					}

					if (statement)
					{
						elseif_declaration->add_child(statement);
					}

					statement->add_child(std::make_shared<occ_ast::body_end>());
				}

				consume(tk_delimiter);
			}
			else
			{
				throw occ_runtime_error(parse_exceptions[EXPECTED_LEFT_BRACE], peek());
			}

			return elseif_declaration;
		}
		else
		{
			throw occ_runtime_error(parse_exceptions[EXPECTED_KEYWORD], peek());
		}
	}

	std::shared_ptr<ast> parser::parse_else()
	{
		if (match(tk_keyword, "else"))
		{
			consume(tk_keyword);

			std::shared_ptr<ast> else_declaration = std::make_shared<occ_ast::else_declaration>();

			if (match(tk_delimiter, "{"))
			{
				consume(tk_delimiter);

				while (!match(tk_delimiter, "}"))
				{
					auto statement = std::make_shared<occ_ast::body_start>();

					statement->add_child(parse_keywords()); // body

					if (match(tk_delimiter, ";"))
					{
						consume(tk_delimiter);
					}

					if (statement)
					{
						else_declaration->add_child(statement);
					}

					statement->add_child(std::make_shared<occ_ast::body_end>());
				}

				consume(tk_delimiter);
			}
			else
			{
				throw occ_runtime_error(parse_exceptions[EXPECTED_LEFT_BRACE], peek());
			}

			return else_declaration;
		}
		else
		{
			throw occ_runtime_error(parse_exceptions[EXPECTED_KEYWORD], peek());
		}
	}

	std::shared_ptr<ast> parser::parse_loop()
	{
		if (match(tk_keyword, "loop"))
		{
			consume(tk_keyword);

			std::shared_ptr<ast> loop_declaration = std::make_shared<occ_ast::loop_declaration>();

			if (match(tk_delimiter, "{"))
			{
				consume(tk_delimiter);

				while (!match(tk_delimiter, "}"))
				{
					auto statement = std::make_shared<occ_ast::body_start>();

					statement->add_child(parse_keywords()); // body

					if (match(tk_delimiter, ";"))
					{
						consume(tk_delimiter);
					}

					if (statement)
					{
						loop_declaration->add_child(statement);
					}

					statement->add_child(std::make_shared<occ_ast::body_end>());
				}

				consume(tk_delimiter);
			}
			else
			{
				throw occ_runtime_error(parse_exceptions[EXPECTED_LEFT_BRACE], peek());
			}

			return loop_declaration;
		}
		else
		{
			throw occ_runtime_error(parse_exceptions[EXPECTED_KEYWORD], peek());
		}
	}

	std::shared_ptr<ast> parser::parse_while()
	{
		if (match(tk_keyword, "while"))
		{
			consume(tk_keyword);

			std::shared_ptr<ast> while_declaration = std::make_shared<occ_ast::while_declaration>();

			auto expression = parse_expression();
			for (auto &child : expression->get_children())
			{
				child->swap_parent(while_declaration);
			}

			if (match(tk_delimiter, "{"))
			{
				consume(tk_delimiter);

				while (!match(tk_delimiter, "}"))
				{
					auto statement = std::make_shared<occ_ast::body_start>();

					statement->add_child(parse_keywords()); // body

					if (match(tk_delimiter, ";"))
					{
						consume(tk_delimiter);
					}

					if (statement)
					{
						while_declaration->add_child(statement);
					}

					statement->add_child(std::make_shared<occ_ast::body_end>());
				}

				consume(tk_delimiter);
			}
			else
			{
				throw occ_runtime_error(parse_exceptions[EXPECTED_LEFT_BRACE], peek());
			}

			return while_declaration;
		}
		else
		{
			throw occ_runtime_error(parse_exceptions[EXPECTED_KEYWORD], peek());
		}
	}

	std::shared_ptr<ast> parser::parse_for()
	{
		if (match(tk_keyword, "for")) 
		{
			consume(tk_keyword);

			auto for_declaration = std::make_shared<occ_ast::for_declaration>();

			auto decl = parse_declaration();

			for_declaration->add_child(decl);

			if (match(tk_keyword, "in")) 
			{
				consume(tk_keyword);
			}

			if (match(tk_identifier))
			{
				auto id = parse_identifier();

				for_declaration->add_child(id);

				if (match(tk_delimiter, "."))
				{
					consume(tk_delimiter);

					if (match(tk_keyword, "step_by")) 
					{
						consume(tk_keyword);

						auto step_by = std::make_shared<occ_ast::step_for>();
					
						if (match(tk_delimiter, "("))
						{
							consume(tk_delimiter);

							auto body_start = std::make_shared<occ_ast::body_start>();

							auto expr = parse_expression();
							for (auto &child : expr->get_children())
							{
								child->swap_parent(body_start);
							}

							step_by->add_child(body_start);

							if (match(tk_delimiter, ")"))
							{
								consume(tk_delimiter);
							}
						}

						id->add_child(step_by);
					}
				}
			}
			else if (match(tk_keyword, "range")) 
			{
				consume(tk_keyword);
				
				auto range = std::make_shared<occ_ast::range_for>();

				if (match(tk_delimiter, "("))
				{
					consume(tk_delimiter);

					auto body_start = std::make_shared<occ_ast::body_start>();

					auto expr = parse_expression();
					for (auto &child : expr->get_children())
					{
						child->swap_parent(body_start);
					}

					range->add_child(body_start);
				}

				for_declaration->add_child(range);
			}

			if (match(tk_delimiter, "{")) 
			{
				consume(tk_delimiter);

				while (!match(tk_delimiter, "}")) 
				{
					auto statement = std::make_shared<occ_ast::body_start>();

					statement->add_child(parse_keywords()); // body

					if (match(tk_delimiter, ";")) 
					{
						consume(tk_delimiter);
					}

					if (statement) 
					{
						for_declaration->add_child(statement);
					}

					statement->add_child(std::make_shared<occ_ast::body_end>());
				}

				if (match(tk_delimiter, "}")) 
				{
					consume(tk_delimiter);
				}
			}
			else 
			{
				throw occ_runtime_error(parse_exceptions[EXPECTED_LEFT_BRACE], peek());
			}

			return for_declaration;
		}
	}

	std::shared_ptr<ast> parser::parse_continue()
	{
		if (match(tk_keyword, "continue"))
		{
			consume(tk_keyword);

			auto continue_stmt = std::make_shared<occ_ast::continue_declaration>();

			return continue_stmt;
		}
		else
		{
			throw occ_runtime_error(parse_exceptions[EXPECTED_BREAK], peek());
		}
	}

	std::shared_ptr<ast> parser::parse_return()
	{
		if (match(tk_keyword, "return"))
		{
			consume(tk_keyword);

			std::shared_ptr<ast> return_statement = std::make_shared<occ_ast::return_declaration>();

			if (match(tk_identifier) && match_next(tk_delimiter, "("))
			{
				auto function_call = std::make_shared<occ_ast::function_call>();

				auto expression = parse_expression();
				for (auto &child : expression->get_children())
				{
					child->swap_parent(function_call);
				}

				return_statement->add_child(function_call);
			}
			else
			{
				auto expression = parse_expression();
				for (auto &child : expression->get_children())
				{
					child->swap_parent(return_statement);
				}
			}

			return return_statement;
		}
	}

	std::shared_ptr<ast> parser::parse_break()
	{
		if (match(tk_keyword, "break"))
		{
			consume(tk_keyword);

			std::shared_ptr<ast> break_statement = std::make_shared<occ_ast::break_declaration>();

			return break_statement;
		}
		else
		{
			throw occ_runtime_error(parse_exceptions[EXPECTED_BREAK], peek());
		}
	}

	std::shared_ptr<ast> parser::parse_include()
	{
		if (match(tk_keyword, "include"))
		{
			consume(tk_keyword);

			if (match(tk_string_literal))
			{
				auto filepath = parse_string_literal()->content;

				if (match(tk_delimiter, ";"))
				{
					consume(tk_delimiter);
				}

				std::string source_original;
				std::ifstream file(filepath);

				if (!file.is_open()) {
					throw std::runtime_error("Can't open file " + filepath);
				}

				std::stringstream ss;
				std::string line;

				while (getline(file, line)) {
					ss << line << "\n";
				}

				source_original = ss.str();
				file.close();

				parser p(source_original);

				return p.parse();
			}
		}
	}

	std::shared_ptr<ast> parser::parse_dereference() 
	{
		if (match(tk_keyword, "deref"))
		{
			consume(tk_keyword);

			auto n = std::make_shared<occ_ast::deref_ptr>();
			
			return n;
		}
	}

	std::shared_ptr<ast> parser::parse_ptr_at() 
	{
		if (match(tk_identifier))
		{
			auto id = parse_identifier();

			if (match(tk_delimiter, ".") && match_next(tk_identifier, "ptr_at"))
			{
				consume(tk_delimiter);
				consume(tk_identifier);

				auto ptr_at = std::make_shared<occ_ast::ptr_at>();

				if (match(tk_delimiter, "("))
				{
					consume(tk_delimiter);

					ptr_at->add_child(id);

					auto n = std::make_shared<occ_ast::body_start>();

					auto expr = parse_expression();
					for (auto &child : expr->get_children())
					{
						child->swap_parent(n);
					}

					ptr_at->add_child(n);

					if (match(tk_operator, "="))
					{
						ptr_at->add_child(parse_assignment());	
					}
				}
				else
					throw occ_runtime_error("no R parenthesis", peek());


				return ptr_at;
			}
			else
			{
				throw occ_runtime_error("ptr_at error", peek());
			}
		}
	}

	//array.get(expr, type); NO ASSIGNMENT
	std::shared_ptr<ast> parser::parse_array_get() 
	{
		if (match(tk_identifier))
		{
			auto id = parse_identifier();

			if (match(tk_delimiter, ".") && match_next(tk_identifier, "get"))
			{
				consume(tk_delimiter);
				consume(tk_identifier);

				auto get = std::make_shared<occ_ast::arr_get>();

				if (match(tk_delimiter, "("))
				{
					consume(tk_delimiter);

					get->add_child(id);

					auto n = std::make_shared<occ_ast::body_start>();

					auto expr = parse_expression();
					for (auto &child : expr->get_children())
					{
						child->swap_parent(n);
					}

					get->add_child(n);

					auto force_end = std::make_shared<occ_ast::force_end>();

					get->add_child(force_end);

					return get;
				}
			}
		}
	}

	// array.add(value); NO ASSIGNMENT
	std::shared_ptr<ast> parser::parse_array_add() 
	{
		if (match(tk_identifier))
		{
			auto id = parse_identifier();

			if (match(tk_delimiter, ".") && match_next(tk_identifier, "add"))
			{
				consume(tk_delimiter);
				consume(tk_identifier);

				auto add = std::make_shared<occ_ast::arr_add>();

				if (match(tk_delimiter, "("))
				{
					consume(tk_delimiter);

					add->add_child(id);

					auto body_start = std::make_shared<occ_ast::body_start>();

					auto expr = parse_expression();
					for (auto &child : expr->get_children())
					{
						child->swap_parent(body_start);
					}

					add->add_child(body_start);

					auto force_end = std::make_shared<occ_ast::force_end>();

					add->add_child(force_end);

					return add;
				}
				else 
					throw occ_runtime_error("expected (", peek());
			}
			else 
				throw occ_runtime_error("expected .add", peek());
		}
		else 
			throw occ_runtime_error("expected identifier", peek());
	}

	// array.set(expr, value); NO ASSIGNMENT could also accept other types of values
	std::shared_ptr<ast> parser::parse_array_set() 
	{
		if (match(tk_identifier))
		{
			auto id = parse_identifier();

			if (match(tk_delimiter, ".") && match_next(tk_identifier, "set"))
			{
				consume(tk_delimiter);
				consume(tk_identifier);

				auto set = std::make_shared<occ_ast::arr_set>();

				if (match(tk_delimiter, "("))
				{
					consume(tk_delimiter);

					set->add_child(id);

					auto n = std::make_shared<occ_ast::body_start>();

					auto expr = parse_expression();
					for (auto &child : expr->get_children())
					{
						child->swap_parent(n);
					}

					set->add_child(n);

					auto force_end = std::make_shared<occ_ast::force_end>();

					set->add_child(force_end);

					return set;
				}
			}
		}
	}

	//array.size(); NO ASSIGNMENT
	std::shared_ptr<ast> parser::parse_array_size() 
	{
		if (match(tk_identifier))
		{
			auto id = parse_identifier();

			if (match(tk_delimiter, ".") && match_next(tk_identifier, "size"))
			{
				consume(tk_delimiter);
				consume(tk_identifier);

				auto size = std::make_shared<occ_ast::arr_size>();

				if (match(tk_delimiter, "("))
				{
					consume(tk_delimiter);

					if (match(tk_delimiter, ")"))
						consume(tk_delimiter);
					else
						throw occ_runtime_error("no R parenthesis", peek());

					size->add_child(id);

					return size;
				}
			}
		}
	}

	/*
		match expr {
			case expr {

			}

			case expr {

			}

			/.../

			default {

			}
		}
	*/

	std::shared_ptr<ast> parser::parse_case() 
	{
		if (match(tk_keyword, "case"))
		{
			consume(tk_keyword);

			auto case_stmt = std::make_shared<occ_ast::case_statement>();

			auto expr_main = parse_expression();
			for (auto &child : expr_main->get_children())
			{
				child->swap_parent(case_stmt);
			}

			if (match(tk_delimiter, "{"))
			{
				consume(tk_delimiter);

				while (!match(tk_delimiter, "}"))
				{
					auto statement = std::make_shared<occ_ast::body_start>();

					statement->add_child(parse_keywords()); // body

					if (match(tk_delimiter, ";"))
					{
						consume(tk_delimiter);
					}

					if (statement)
					{
						case_stmt->add_child(statement);
					}

					statement->add_child(std::make_shared<occ_ast::body_end>());
				}

				consume(tk_delimiter);
			}

			return case_stmt;
		}
	}

	std::shared_ptr<ast> parser::parse_compiler_breakpoint() 
	{
		if (match(tk_keyword, "compilerbreakpoint"))
		{
			consume(tk_keyword);

			auto breakpoint = std::make_shared<occ_ast::compilerbreakpoint>();

			breakpoint->content = "Stopping Compiler (break)!\nLine: " + std::to_string(peek().get_line()) + "\nColumn: " + std::to_string(peek().get_column());
			
			return breakpoint;
		}
	}

	std::shared_ptr<ast> parser::parse_default() 
	{
		if (match(tk_keyword, "default"))
		{
			consume(tk_keyword);

			auto default_stmt = std::make_shared<occ_ast::default_statement>();

			if (match(tk_delimiter, "{"))
			{
				consume(tk_delimiter);

				while (!match(tk_delimiter, "}"))
				{
					auto statement = std::make_shared<occ_ast::body_start>();

					statement->add_child(parse_keywords()); // body

					if (match(tk_delimiter, ";"))
					{
						consume(tk_delimiter);
					}

					if (statement)
					{
						default_stmt->add_child(statement);
					}

					statement->add_child(std::make_shared<occ_ast::body_end>());
				}

				consume(tk_delimiter);
			}

			return default_stmt;
		}
	}

	std::shared_ptr<ast> parser::parse_match() 
	{
		if (match(tk_keyword, "match"))
		{
			consume(tk_keyword);

			auto match_stmt = std::make_shared<occ_ast::match_statement>();

			auto expr_main = parse_expression();
			for (auto &child : expr_main->get_children())
			{
				child->swap_parent(match_stmt);
			}

			if (match(tk_delimiter, "{"))
			{
				consume(tk_delimiter);

				while (!match(tk_delimiter, "}"))
				{
					if (match(tk_keyword, "case")) 
					{
						auto case_stmt = parse_case();
						match_stmt->add_child(case_stmt);
					}
					else if (match(tk_keyword, "default"))
					{
						auto default_stmt = parse_default();
						match_stmt->add_child(default_stmt);
					}
				}

				consume(tk_delimiter);
			}

			return match_stmt;
		}
	}

	std::shared_ptr<ast> parser::parse_keywords()
	{		
		if (match(tk_keyword, "if"))
		{
			return parse_if();
		}
		else if (match(tk_keyword, "compilerbreakpoint"))
		{
			return parse_compiler_breakpoint();
		}
		else if (match(tk_keyword, "else") && match_next(tk_keyword, "if"))
		{
			return parse_elseif();
		}
		else if (match(tk_keyword, "else"))
		{
			return parse_else();
		}
		else if (match(tk_keyword, "match"))
		{
			return parse_match();
		}
		else if (match(tk_keyword, "loop"))
		{
			return parse_loop();
		}
		else if (match(tk_keyword, "return"))
		{
			return parse_return();
		}
		else if (match(tk_keyword, "while"))
		{
			return parse_while();
		}
		else if (match(tk_keyword, "for"))
		{
			return parse_for();
		}
		else if (match(tk_keyword, "continue"))
		{
			return parse_continue();
		}
		else if (match(tk_keyword, "break"))
		{
			return parse_break();
		}
		else if (match(tk_keyword, "array"))
		{
			return parse_declaration();
		}
		else if (match(tk_keyword, "array") && match_next(tk_keyword, "ptr"))
		{
			return parse_declaration();
		}
		else if (match(tk_keyword, "deref"))
		{
			return parse_dereference();
		}
		else if (match(tk_keyword, "rnum") && match_next(tk_keyword, "ptr")) // order matters here, ptr has to be first
		{
			return parse_declaration();
		}
		else if (match(tk_keyword, "float") && match_next(tk_keyword, "ptr"))
		{
			return parse_declaration();
		}
		else if (match(tk_keyword, "int") && match_next(tk_keyword, "ptr"))
		{
			return parse_declaration();
		}
		else if (match(tk_keyword, "num") && match_next(tk_keyword, "ptr"))
		{
			return parse_declaration();
		}
		else if (match(tk_keyword, "str") && match_next(tk_keyword, "ptr"))
		{
			return parse_declaration();
		}
		else if (match(tk_keyword, "void") && match_next(tk_keyword, "ptr"))
		{
			return parse_declaration();
		}
		else if (match(tk_keyword, "rnum") || match(tk_keyword, "float"))
		{
			return parse_declaration();
		}
		else if (match(tk_keyword, "num") || match(tk_keyword, "int"))
		{
			return parse_declaration();
		}
		else if (match(tk_keyword, "bool"))
		{
			return parse_declaration();
		}
		else if (match(tk_keyword, "str"))
		{
			return parse_declaration();
		}
		else if (match(tk_identifier) && match_next(tk_delimiter, ".") && peek(2).get_lexeme() == "ptr_at")
		{
			return parse_ptr_at();
		}
		else if (match(tk_identifier) && match_next(tk_delimiter, ".") && peek(2).get_lexeme() == "get")
		{
			return parse_array_get();
		}
		else if (match(tk_identifier) && match_next(tk_delimiter, ".") && peek(2).get_lexeme() == "add")
		{
			return parse_array_add();
		}
		else if (match(tk_identifier) && match_next(tk_delimiter, ".") && peek(2).get_lexeme() == "set")
		{
			return parse_array_set();
		}
		else if (match(tk_identifier) && match_next(tk_delimiter, ".") && peek(2).get_lexeme() == "size")
		{
			return parse_array_size();
		}
		else if (match(tk_identifier) && match_next(tk_operator, "++"))
		{
			auto postfix_or_prefix = std::make_shared<occ_ast::postfix_or_prefix>();

			auto id = parse_identifier();
			auto op = consume(tk_operator).get_lexeme();

			auto op_decl = std::make_shared<occ_ast::operator_declaration>();
			op_decl->content = op;

			postfix_or_prefix->add_child(id);
			postfix_or_prefix->add_child(op_decl);

			return postfix_or_prefix;
		}
		else if (match(tk_identifier) && match_next(tk_operator, "--"))
		{
			auto postfix_or_prefix = std::make_shared<occ_ast::postfix_or_prefix>();

			auto id = parse_identifier();
			auto op = consume(tk_operator).get_lexeme();

			auto op_decl = std::make_shared<occ_ast::operator_declaration>();
			op_decl->content = op;

			postfix_or_prefix->add_child(id);
			postfix_or_prefix->add_child(op_decl);

			return postfix_or_prefix;
		}
		else if (match(tk_operator, "++") && match_next(tk_identifier))
		{
			auto postfix_or_prefix = std::make_shared<occ_ast::postfix_or_prefix>();

			auto op = consume(tk_operator).get_lexeme();
			auto id = parse_identifier();

			auto op_decl = std::make_shared<occ_ast::operator_declaration>();
			op_decl->content = op;

			postfix_or_prefix->add_child(op_decl);
			postfix_or_prefix->add_child(id);

			return postfix_or_prefix;
		}
		else if (match(tk_operator, "--") && match_next(tk_identifier))
		{
			auto postfix_or_prefix = std::make_shared<occ_ast::postfix_or_prefix>();

			auto op = consume(tk_operator).get_lexeme();
			auto id = parse_identifier();

			auto op_decl = std::make_shared<occ_ast::operator_declaration>();
			op_decl->content = op;

			postfix_or_prefix->add_child(op_decl);
			postfix_or_prefix->add_child(id);

			return postfix_or_prefix;
		}
		else if (match(tk_identifier) && match_next(tk_delimiter, "("))
		{
			auto function_call = std::make_shared<occ_ast::function_call>();

			auto expression = parse_expression();
			for (auto &child : expression->get_children())
			{
				child->swap_parent(function_call);
			}

			return function_call;
		}
		else if (match(tk_identifier) && match_next(tk_operator, "=")) // assignment to variable
		{
			auto v = parse_identifier();

			if (match(tk_operator, "="))
			{
				consume(tk_operator);

				std::shared_ptr<ast> assignment = std::make_shared<occ_ast::assignment>();

				if (match(tk_identifier) && match_next(tk_delimiter, "("))
				{
					auto function_call = std::make_shared<occ_ast::function_call>();

					auto expression = parse_expression();
					for (auto &child : expression->get_children())
					{
						child->swap_parent(function_call);
					}

					assignment->add_child(function_call);
				} 
				else
				{
					auto expression = parse_expression();
					for (auto &child : expression->get_children())
					{
						child->swap_parent(assignment);
					}
				}

				v->add_child(assignment);
			}

			return v;
		}
		else
		{
			throw occ_runtime_error(parse_exceptions[EXPECTED_KEYWORD], peek());
		}
	}

	std::shared_ptr<ast> parser::_parse_delaration(std::optional<std::shared_ptr<ast>> variable_declaration)
	{
		if (variable_declaration.has_value())
		{
			if (match(tk_delimiter, ":") && match_next(tk_identifier))
			{
				consume(tk_delimiter);

				variable_declaration.value()->add_child(parse_identifier());
			}

			if (match(tk_identifier))
			{
				variable_declaration.value()->add_child(parse_identifier());
			}

			if (match(tk_operator, "="))
			{
				variable_declaration.value()->add_child(parse_assignment());
			}

			return variable_declaration.value();
		}
		else
		{
			throw occ_runtime_error(parse_exceptions[EXPECTED_KEYWORD], peek());
		}
	}

	std::shared_ptr<ast> parser::parse_declaration()
	{
		if ((match(tk_keyword, "rnum") || match(tk_keyword, "float")) && match_next(tk_keyword, "ptr")) // order matters here ptr has to be first
		{
			consume(tk_keyword);
			consume(tk_keyword);

			return _parse_delaration(std::make_shared<occ_ast::rnum_ptr_declaration>());
		}
		else if ((match(tk_keyword, "num") || match(tk_keyword, "int")) && match_next(tk_keyword, "ptr"))
		{
			consume(tk_keyword);
			consume(tk_keyword);

			return _parse_delaration(std::make_shared<occ_ast::num_ptr_declaration>());
		}
		else if (match(tk_keyword, "str") && match_next(tk_keyword, "ptr"))
		{
			consume(tk_keyword);
			consume(tk_keyword);

			return _parse_delaration(std::make_shared<occ_ast::str_ptr_declaration>());
		}
		else if (match(tk_keyword, "void") && match_next(tk_keyword, "ptr"))
		{
			consume(tk_keyword);
			consume(tk_keyword);

			return _parse_delaration(std::make_shared<occ_ast::void_ptr_declaration>());
		}
		else if (match(tk_keyword, "rnum") || match(tk_keyword, "float"))
		{
			consume(tk_keyword);

			return _parse_delaration(std::make_shared<occ_ast::float_declaration>());
		}
		else if (match(tk_keyword, "num") || match(tk_keyword, "int"))
		{
			consume(tk_keyword);

			return _parse_delaration(std::make_shared<occ_ast::num_declaration>());
		}
		else if (match(tk_keyword, "bool"))
		{
			consume(tk_keyword);

			return _parse_delaration(std::make_shared<occ_ast::bool_declaration>());
		}
		else if (match(tk_keyword, "str"))
		{
			consume(tk_keyword);

			return _parse_delaration(std::make_shared<occ_ast::string_declaration>());
		}
		else if (match(tk_keyword, "array") && match_next(tk_keyword, "ptr")) 
		{
			consume(tk_keyword);
			consume(tk_keyword);

			return _parse_delaration(std::make_shared<occ_ast::array_ptr_declaration>());
		}
		else if (match(tk_keyword, "array")) 
		{
			consume(tk_keyword);
			
			auto arr_decl = std::make_shared<occ_ast::array_declaration>();
			
			if (match(tk_operator, "<")) 
			{
				consume(tk_operator);
				
				if (match(tk_keyword)) 
				{
					auto keyword = consume(tk_keyword).get_lexeme();

					if (keyword == "num" || keyword == "int")
					{
						arr_decl->add_child(std::make_shared<occ_ast::num_declaration>());
					}
					else if (keyword == "rnum" || keyword == "float")
					{
						arr_decl->add_child(std::make_shared<occ_ast::float_declaration>());
					}
					else if (keyword == "str")
					{
						arr_decl->add_child(std::make_shared<occ_ast::string_declaration>());
					}
					else if (keyword == "bool")
					{
						arr_decl->add_child(std::make_shared<occ_ast::bool_declaration>());
					}
					else if (keyword == "array" || keyword == "generic")
					{
						arr_decl->add_child(std::make_shared<occ_ast::array_declaration>());
					}

					if (match(tk_operator, ">")) 
					{
						consume(tk_operator);

						if (match(tk_identifier))
						{
							auto id = parse_identifier();

							arr_decl->get_child()->add_child(id);
							
							if (match(tk_operator, "="))
							{
								consume(tk_operator);

								auto expression = parse_expression();
								for (auto &child : expression->get_children())
								{
									child->swap_parent(arr_decl->get_child()->get_child());
								}
							}
						}

						return arr_decl;
					}
					else 
					{
						throw occ_runtime_error(parse_exceptions[EXPECTED_DELIMITER], peek());
					}
				}
			}
			else if (match(tk_delimiter, "{"))
			{
				return _parse_delaration(std::make_shared<occ_ast::array_declaration>());
			}
			else if (match(tk_delimiter, ":") && match_next(tk_identifier))
			{
				consume(tk_delimiter);
				
				auto id = parse_identifier();

				arr_decl->add_child(id);

				if (match(tk_operator, "="))
				{
					consume(tk_operator);

					if (match(tk_identifier) && match_next(tk_delimiter, "("))
					{
						auto function_call = std::make_shared<occ_ast::function_call>();

						auto expression = parse_expression();
						for (auto &child : expression->get_children())
						{
							child->swap_parent(function_call);
						}

						arr_decl->get_child()->add_child(function_call);
					}
					else
					{
						auto expression = parse_expression();
						for (auto &child : expression->get_children())
						{
							child->swap_parent(arr_decl->get_child()->get_child());
						}
					}
				}

				return arr_decl;
			}
			else
			{
				throw occ_runtime_error(parse_exceptions[EXPECTED_DELIMITER], peek());
			}
		}
		else if (match(tk_keyword, "void"))
		{
			throw occ_runtime_error("voids cant have a variable", peek());
		}
		else
		{
			throw occ_runtime_error(parse_exceptions[EXPECTED_TYPE], peek());
		}
	}

	std::vector<std::shared_ptr<ast>> parser::parse_term() // does this work with i++?
	{
		std::vector<std::shared_ptr<ast>> vec;

		while (true)
		{	
			if (match(tk_delimiter, ";") || match(tk_delimiter, "{"))
			{
				break;
			}
			else if (match(tk_keyword, "deref")) 
			{
				auto node = parse_dereference();
				vec.push_back(node);
			}
			else if (match(tk_identifier) && match_next(tk_delimiter, ".") && peek(2).get_lexeme() == "ptr_at")
			{
				auto n = parse_ptr_at();
				vec.push_back(n);
			}
			else if (match(tk_identifier) && match_next(tk_delimiter, ".") && peek(2).get_lexeme() == "get")
			{
				auto n = parse_array_get();
				vec.push_back(n);
			}
			else if (match(tk_identifier) && match_next(tk_delimiter, ".") && peek(2).get_lexeme() == "set")
			{
				auto n = parse_array_set();
				vec.push_back(n);
			}
			else if (match(tk_identifier) && match_next(tk_delimiter, ".") && peek(2).get_lexeme() == "add")
			{
				auto n = parse_array_add();
				vec.push_back(n);
			}
			else if (match(tk_identifier) && match_next(tk_delimiter, ".") && peek(2).get_lexeme() == "size")
			{
				auto n = parse_array_size();
				vec.push_back(n);
			}
			else if (match(tk_identifier))
			{
				auto node = parse_identifier();
				vec.push_back(node);
			}
			else if (match(tk_number_literal))
			{
				auto node = parse_number_literal();
				vec.push_back(node);
			}
			else if (match(tk_float_literal))
			{
				auto node = parse_float_literal();
				vec.push_back(node);
			}
			else if (match(tk_operator))
			{
				auto node = std::make_shared<occ_ast::operator_declaration>();
				node->content = consume(tk_operator).get_lexeme();
				vec.push_back(node);
			}
			else if (match(tk_delimiter, "(") || match(tk_delimiter, ")"))
			{
				auto node = std::make_shared<occ_ast::delimiter_declaration>();
				node->content = consume(tk_delimiter).get_lexeme();
				vec.push_back(node);
			}
			else if (match(tk_delimiter, "[") || match(tk_delimiter, "]"))
			{
				auto node = std::make_shared<occ_ast::delimiter_declaration>();
				node->content = consume(tk_delimiter).get_lexeme();
				vec.push_back(node);
			}
			else if (match(tk_string_literal))
			{
				vec.push_back(parse_string_literal());
			}
			else if (match(tk_boolean_literal))
			{
				vec.push_back(parse_boolean_literal());
			}
			else if (match(tk_keyword, "true") || match(tk_keyword, "false"))
			{
				auto boolean_literal = std::make_shared<occ_ast::boolean_literal>();
				boolean_literal->content = consume(tk_keyword).get_lexeme();
				vec.push_back(boolean_literal);
			}
			else if (match(tk_delimiter, ","))
			{
				consume(tk_delimiter);
				auto comma_ast = std::make_shared<occ_ast::comma>();
				vec.push_back(comma_ast);
			}
			else
			{
				break;
			}
		}

		return vec;
	}

	std::shared_ptr<ast> parser::parse_expression()
	{
		std::shared_ptr<ast> expression = std::make_shared<ast>();

		auto expr = parse_term();
		for (int i = 0; i < expr.size(); i++)
		{
			auto content = expr[i]->content;
			expression->add_child(expr[i]);
		}

		return expression;
	}

	std::shared_ptr<ast> parser::parse_function()
	{
		if (match(tk_keyword, "fn"))
		{
			std::shared_ptr<ast> function_declaration = std::make_shared<occ_ast::function_declaration>();

			consume(tk_keyword);

			function_declaration->add_child(parse_identifier());

			if (match(tk_delimiter, "("))
			{
				consume(tk_delimiter);

				while (!match(tk_delimiter, ")"))
				{
					std::shared_ptr<ast> var_decl = std::make_shared<ast>();

					if (match(tk_keyword))
					{
						var_decl = parse_declaration();

						function_declaration->add_child(var_decl);
					}
					else
					{
						throw occ_runtime_error(parse_exceptions[EXPECTED_RETURN_TYPE], peek());
					}

					if (match(tk_delimiter, ","))
					{
						consume(tk_delimiter);
					}
				}

				consume(tk_delimiter);
			}
			else
			{
				throw occ_runtime_error(parse_exceptions[EXPECTED_LEFT_PARENTHESIS], peek());
			}

			if (match(tk_delimiter, "->"))
			{
				consume(tk_delimiter);
			}

			if (match(tk_keyword))
			{
				function_declaration->add_child(parse_declaration());
			}
			else
			{ // if no return type is specified, it is void
				function_declaration->add_child(std::make_shared<occ_ast::void_declaration>());
			}

			if (match(tk_delimiter, "{"))
			{
				consume(tk_delimiter);

				while (!match(tk_delimiter, "}"))
				{
					auto statement = std::make_shared<occ_ast::body_start>();

					statement->add_child(parse_keywords()); // body

					if (match(tk_delimiter, ";"))
					{
						consume(tk_delimiter);
					}

					if (statement)
					{
						function_declaration->add_child(statement);
					}

					statement->add_child(std::make_shared<occ_ast::body_end>());
				}

				consume(tk_delimiter);
			}
			else
			{
				throw occ_runtime_error(parse_exceptions[EXPECTED_LEFT_BRACE], peek());
			}

			return function_declaration;
		}
		else
		{
			throw occ_runtime_error(parse_exceptions[EXPECTED_FUNCTION], peek());
		}
	}

	std::shared_ptr<ast> parser::parse_number_literal()
	{
		if (match(tk_number_literal))
		{
			std::shared_ptr<ast> number = std::make_shared<occ_ast::number_literal>();

			number->content = consume(tk_number_literal).get_lexeme();

			return number;
		}
		else
		{
			throw occ_runtime_error(parse_exceptions[EXPECTED_NUMBER_LITERAL], peek());
		}
	}

	std::shared_ptr<ast> parser::parse_float_literal()
	{
		if (match(tk_float_literal))
		{
			std::shared_ptr<ast> number = std::make_shared<occ_ast::float_literal>();

			number->content = consume(tk_float_literal).get_lexeme();

			return number;
		}
		else
		{
			throw occ_runtime_error("Expected float literal", peek());
		}
	}

	std::shared_ptr<ast> parser::parse_string_literal()
	{
		if (match(tk_string_literal))
		{
			std::shared_ptr<ast> string_literal = std::make_shared<occ_ast::string_literal>();

			string_literal->content = consume(tk_string_literal).get_lexeme();

			return string_literal;
		}
		else
		{
			throw occ_runtime_error(parse_exceptions[EXPECTED_STRING_LITERAL], peek());
		}
	}

	std::shared_ptr<ast> parser::parse_boolean_literal()
	{
		if (match(tk_boolean_literal))
		{
			std::shared_ptr<ast> boolean_literal = std::make_shared<occ_ast::boolean_literal>();

			boolean_literal->content = consume(tk_boolean_literal).get_lexeme();

			return boolean_literal;
		}
		else
		{
			throw occ_runtime_error(parse_exceptions[EXPECTED_BOOLEAN_LITERAL], peek());
		}
	}

	void parser::clean_comments()
	{
		tokens.erase(std::remove_if(tokens.begin(), tokens.end(), [](const token& tk) { return tk.get_type() == tk_comment; }), tokens.end());
	}

	std::shared_ptr<ast> parser::parse()
	{
		clean_comments();

		std::shared_ptr<ast> root = std::make_shared<ast>();

		while (position < tokens.size() && tokens[position].get_type() != tk_eof)
		{
			auto statement = std::shared_ptr<ast>();
			bool isimport = false;
			
			if (match(tk_keyword, "include"))
			{
				statement = parse_include();
				isimport = true;
			}
			else if (match(tk_keyword, "fn"))
			{
				statement = parse_function();
			}

			if (isimport) 
			{
				for (auto &child : statement->get_children())
				{
					child->swap_parent(root);
				}
			}
			else if (statement)
			{
				root->add_child(statement);
			}
		}

		return root;
	}
} // occultlang