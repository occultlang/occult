#pragma once
#include <memory>
#include <vector>

namespace occultlang {
    template<typename T>
    class tree : public std::enable_shared_from_this<tree<T>> {
        T value;
        std::vector<std::shared_ptr<tree<T>>> children;
        std::weak_ptr<tree<T>> parent;
    public:
        tree(const T& value) : value(value), parent(), children({}) {}

        void add_child(std::shared_ptr<tree<T>> child) {
            child->parent = this->shared_from_this();
            children.push_back(child);
        }

        void add_parent(std::shared_ptr<tree<T>> new_parent) {
            parent = new_parent;
            new_parent->add_child(this->shared_from_this());
        }

        std::shared_ptr<tree<T>> get_parent() {
            return parent.lock();
        }

        std::vector<std::shared_ptr<tree<T>>> get_children() {
            return children;
        }

        std::shared_ptr<tree<T>> get_child(std::intptr_t index = 0) {
            return children[index];
        }

        T get_value() {
            return value;
        }
    };
}