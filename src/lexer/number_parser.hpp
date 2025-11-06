#pragma once
#include <charconv>
#include <string>
#include <stdexcept>
#include <optional>
#include <cstdint>

#define BINARY_BASE 2
#define DECIMAL_BASE 10
#define OCTAL_BASE 8
#define HEX_BASE 16

namespace occult {
    inline bool is_octal(const char &c) { return (c >= '0' && c <= '7'); }

    inline bool is_hex(const char &c) {
        return (std::isdigit(c) || (std::tolower(c) >= 'a' && std::tolower(c) <= 'f'));
    }

    inline bool is_binary(const char &c) { return (c == '0' || c == '1'); }

    template<typename ValueType>
    constexpr std::string to_parsable_type(const std::string &number,
                                           const std::optional<std::uintptr_t> base = std::nullopt) {
        ValueType value;

        if constexpr (std::is_floating_point_v<ValueType>) {
            if (auto result = std::from_chars(number.data(), number.data() + number.size(), value);
                result.ec != std::errc()) { throw std::runtime_error("failed parsing number"); }
        }
        else if constexpr (std::is_integral_v<ValueType>) {
            if (!base.has_value()) { throw std::runtime_error("base is required parsing integers"); }

            auto result = std::from_chars(number.data(), number.data() + number.size(), value,
                                          static_cast<int>(base.value()));

            if (result.ec != std::errc()) { throw std::runtime_error("failed parsing number"); }
        }

        return std::to_string(value);
    }

    template<typename ValueType>
    constexpr ValueType from_numerical_string(const std::string &number) {
        ValueType value;

        if constexpr (std::is_floating_point_v<ValueType>) {
            if (auto result = std::from_chars(number.data(), number.data() + number.size(), value);
                result.ec != std::errc()) { throw std::runtime_error("failed parsing number"); }
        }
        else if constexpr (std::is_integral_v<ValueType>) {
            if (auto result = std::from_chars(number.data(), number.data() + number.size(), value, 10);
                result.ec != std::errc()) { throw std::runtime_error("failed parsing number"); }
        }

        return value;
    }
} // namespace occult
