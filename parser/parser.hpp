#pragma once
#include "ast.hpp"
#include "../lexer/lexer.hpp"
#include <vector>
#include <variant>
#include <expected>
#include <functional>

constexpr std::int8_t max_i8 = std::numeric_limits<std::int8_t>::max();
constexpr std::int8_t min_i8 = std::numeric_limits<std::int8_t>::min();

constexpr std::int16_t max_i16 = std::numeric_limits<std::int16_t>::max();
constexpr std::int16_t min_i16 = std::numeric_limits<std::int16_t>::min();

constexpr std::int32_t max_i32 = std::numeric_limits<std::int32_t>::max();
constexpr std::int32_t min_i32 = std::numeric_limits<std::int32_t>::min();

constexpr std::int64_t max_i64 = std::numeric_limits<std::int64_t>::max();
constexpr std::int64_t min_i64 = std::numeric_limits<std::int64_t>::min();

constexpr std::uint8_t max_u8 = std::numeric_limits<std::uint8_t>::max();

constexpr std::uint16_t max_u16 = std::numeric_limits<std::uint16_t>::max();

constexpr std::uint32_t max_u32 = std::numeric_limits<std::uint32_t>::max();

constexpr std::uint64_t max_u64 = std::numeric_limits<std::uint64_t>::max();

enum class parse_error {
	INT_OUT_OF_BOUNDS,
	EXPECTED_INTEGER,
	EXPECTED_CLOSE,
	TYPE_DOESNT_EXIST,
	EXPECTED_OPEN,
};

const std::unordered_set<std::string> data_type_set = {
	"i8", "i16", "i32", "i64",
	"u8", "u16", "u32", "u64",
	"f32", "f64", "bool", "char", "string", "void"
};

namespace occultlang {
	class parser {
		lexer _lexer;
		std::vector<token> tokens;
		int position;
		std::unordered_set<std::string> user_data_type_set;
	public:
		parser(const std::string& source) : _lexer(source) { tokens = _lexer.lex(); }

		token consume(token_type tt) {
			if (position < tokens.size() && tokens[position].get_type() == tt) {
				return tokens[position++];
			}
		}

		token consume(token_type tt, token_type tt2) {
			if (position < tokens.size() && tokens[position].get_type() == tt || tokens[position].get_type() == tt2) {
				return tokens[position++];
			}
		}

		token peek() {
			return tokens[position];
		}

		bool match(token_type tt, std::string match) { 
			return (tokens[position].get_type() == tt && tokens[position].get_lexeme() == match) ? true : false;
		}

		bool match(token_type tt) {
			return (tokens[position].get_type() == tt) ? true : false;
		}

		std::vector<token>& get_tokens() { return tokens; }

		std::expected<std::shared_ptr<ast>, parse_error> parse_statement();
		std::expected<std::shared_ptr<ast>, parse_error> parse_struct_body();
		std::expected<std::shared_ptr<ast>, parse_error> parse_function_argument(const std::string& keyword);
		std::expected<std::vector<std::shared_ptr<ast>>, parse_error> parse_function_arguments();
		std::expected<std::shared_ptr<ast>, parse_error> parse_factor();

		template<typename AstType>
		std::expected<std::shared_ptr<ast>, parse_error> parse_term(const std::string& operation, std::function<std::expected<std::shared_ptr<ast>, parse_error>()> parse_next);

		std::expected<std::shared_ptr<ast>, parse_error> parse_multiplication();
		std::expected<std::shared_ptr<ast>, parse_error> parse_division();
		std::expected<std::shared_ptr<ast>, parse_error> parse_modulo();
		std::expected<std::shared_ptr<ast>, parse_error> parse_addition();
		std::expected<std::shared_ptr<ast>, parse_error> parse_subtraction();

		std::shared_ptr<ast> parse();
	};
} // occultlang