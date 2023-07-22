#pragma once
#include <vector>
#include <string>
#include <memory>
#include <cstdint>
#include <limits>
#include <cmath>
#include "../data structures/tree.hpp"

namespace occultlang {
	class ast : public tree<std::shared_ptr<ast>> {
	public:
		ast() : tree<std::shared_ptr<ast>>(nullptr) {}
		virtual ~ast() {}
		virtual std::string to_string();

		static void visualize(std::shared_ptr<ast> tree, int indent = 0);
	};

	class ast_string_literal : public ast {
		std::string value;

		std::string get_value();
		std::string to_string() override;
	};

	class ast_boolean_literal : public ast {
		bool value;

		bool get_value();
		std::string to_string() override;
	};

	class ast_8int_literal : public ast {
		std::int8_t value;
	public:
		ast_8int_literal(std::int8_t value) : value(value) {}

		std::int8_t get_value();
		std::string to_string() override;
	};

	class ast_data_type : public ast { // add get_value method
		std::string value;
	public:
		ast_data_type(std::string value) : value(value) {}
	public:
		std::string to_string() override;
	};

	class ast_16int_literal : public ast {
		std::int16_t value;
	public:
		ast_16int_literal(std::int16_t value) : value(value) {}

		std::int16_t get_value();
		std::string to_string() override;
	};

	class ast_32int_literal : public ast {
		std::int32_t value;
	public:
		ast_32int_literal(std::int32_t value) : value(value) {}

		std::int32_t get_value();
		std::string to_string() override;
	};

	class ast_64int_literal : public ast {
		std::int64_t value;
	public:
		ast_64int_literal(std::int64_t value) : value(value) {}

		std::int64_t get_value();
		std::string to_string() override;
	};

	class ast_8uint_literal : public ast {
		std::uint8_t value;
	public:
		ast_8uint_literal(std::uint8_t value) : value(value) {}

		std::uint8_t get_value();
		std::string to_string() override;
	};

	class ast_16uint_literal : public ast {
		std::uint16_t value;
	public:
		ast_16uint_literal(std::uint16_t value) : value(value) {}

		std::uint16_t get_value();
		std::string to_string() override;
	};

	class ast_32uint_literal : public ast {
		std::uint32_t value;
	public:
		ast_32uint_literal(std::uint32_t value) : value(value) {}

		std::uint32_t get_value();
		std::string to_string() override;
	};

	class ast_64uint_literal : public ast {
		std::uint64_t value;
	public:
		ast_64uint_literal(std::uint64_t value) : value(value) {}

		std::uint64_t get_value();
		std::string to_string() override;
	};
	class ast_32float_literal : public ast {
		std::float_t value;

		std::float_t get_value();
		std::string to_string() override;
	};

	class ast_64float_literal : public ast {
		std::double_t value;

		std::double_t get_value();
		std::string to_string() override;
	};

	class ast_identifier : public ast {
		std::string name;
	public:
		ast_identifier(const std::string& name) : name(name) {}

		std::string get_name();
		std::string to_string() override;
	};

	class ast_variable_declaration : public ast {
	public:
		std::string to_string() override;
	};

	class ast_assignment : public ast {
	public:
		std::string to_string() override;
	};

	class ast_multiplication : public ast {
	public:
		std::string to_string() override;
	};

	class ast_division : public ast {
	public:
		std::string to_string() override;
	};

	class ast_addition : public ast {
	public:
		std::string to_string() override;
	};

	class ast_subtraction : public ast {
	public:
		std::string to_string() override;
	};

	class ast_modulo : public ast {
	public:
		std::string to_string() override;
	};

	class ast_function_declaration : public ast {
	public:
		std::string to_string() override;
	};

	class ast_function_argument_declaration : public ast {
	public:
		std::string to_string() override;
	};

	class ast_struct_declaration : public ast {
	public:
		std::string to_string() override;
	};

	class ast_enum_value : public ast {
	private:
		std::string name;
	public:
		ast_enum_value(const std::string& name) : name(name) {}

		std::string to_string() override { return name; }
	};

	class ast_enum_declaration : public ast {
	public:
		std::string to_string() override { return "enum declaration"; }
	};
} // occultlang