#include "writer.hpp"

namespace occult {

void writer::push_byte(const std::uint8_t& byte) { code.emplace_back(byte); }

void writer::push_bytes(const std::initializer_list<std::uint8_t>& bytes) {
    code.insert(code.end(), bytes);
}

void writer::push_bytes(const std::vector<std::uint8_t>& bytes) {
    code.insert(code.end(), bytes.begin(), bytes.end());
}

std::vector<std::uint8_t> writer::string_to_bytes(const std::string& str) {
    std::vector<std::uint8_t> vectorized_data;
    vectorized_data.reserve(str.size());
    for (const auto& byte : str) {
        vectorized_data.emplace_back(static_cast<std::uint8_t>(byte));
    }
    return vectorized_data;
}

std::size_t writer::push_string(const std::string& str) {
    if (!string_locations.contains(str)) {
        string_locations.insert({str, code.size()});
    }
    const auto initial = code.size();
    push_bytes(string_to_bytes(str));
    return initial + 1;
}

#ifdef __linux__
jit_function writer::setup_function() {
    const std::size_t required_size = code.size();

    if (required_size > allocated_size) {
        const std::size_t new_size = ((required_size / page_size) + 1) * page_size;
        void* new_memory = mremap(memory, allocated_size, new_size, MREMAP_MAYMOVE);
        if (new_memory == MAP_FAILED) {
            throw std::runtime_error("Failed to remap memory");
        }
        memory = new_memory;
        allocated_size = new_size;
    }

    std::memcpy(memory, code.data(), required_size);

    const std::size_t aligned_size = ((required_size + page_size - 1) / page_size) * page_size;
    if (mprotect(memory, aligned_size, PROT_READ | PROT_EXEC) != 0) {
        throw std::runtime_error("Failed to set memory executable");
    }

    return reinterpret_cast<jit_function>(memory);
}
#endif

#ifdef _WIN64
jit_function writer::setup_function() {
    const std::size_t required_size = code.size();

    if (required_size > allocated_size) {
        const std::size_t new_size = ((required_size / page_size) + 1) * page_size;

        PVOID new_memory =
            VirtualAlloc(nullptr, new_size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
        if (new_memory == nullptr) {
            throw std::runtime_error("Failed to allocate additional memory");
        }

        std::memcpy(new_memory, memory, code.size());

        VirtualFree(memory, 0, MEM_RELEASE);

        memory = new_memory;
        allocated_size = new_size;
    } else {
        std::memcpy(memory, code.data(), required_size);
    }

    return reinterpret_cast<jit_function>(memory);
}
#endif

std::vector<std::uint8_t>& writer::get_code() { return code; }

void writer::print_bytes() const {
    for (auto& c : code) {
        std::cout << std::setw(2) << std::setfill('0') << std::uppercase << std::hex
                  << static_cast<int>(c) << " " << std::dec;
    }
    std::cout << "\n";
}

const std::size_t& writer::get_string_location(const std::string& str) {
    return string_locations[str];
}

} // namespace occult