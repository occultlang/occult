#pragma once
#include "cst.hpp"

namespace occult {
    // Resolve every `type Name = <datatype>;` in the tree.
    // Substitutes identifier type-references with their aliased type;
    // detects cycles and throws.
    void resolve_type_aliases(cst* root);
}
