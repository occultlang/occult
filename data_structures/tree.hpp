#pragma once
#include <memory>
#include <vector>
#include <algorithm>

namespace occultlang
{
    template <typename T>
    class tree : public std::enable_shared_from_this<tree<T>>
    {
        T value;
        std::vector<std::shared_ptr<tree<T>>> children;
        std::weak_ptr<tree<T>> parent;
    public:
        tree(const T &value) : value(value), parent(), children({}) {}
        virtual ~tree() {}

        void add_child(std::shared_ptr<tree<T>> child)
        {
            child->parent = this->shared_from_this();
            children.push_back(child);
        }

        void add_parent(std::shared_ptr<tree<T>> new_parent)
        {
            parent = new_parent;
            new_parent->add_child(this->shared_from_this());
        }

        std::shared_ptr<tree<T>> get_parent()
        {
            return parent.lock();
        }

        std::vector<std::shared_ptr<tree<T>>> get_children()
        {
            return children;
        }

        std::shared_ptr<tree<T>> get_child(int index = 0)
        {
            return children[index];
        }

        void swap_parent(std::shared_ptr<tree<T>> new_parent)
        {
            if (auto old_parent = parent.lock())
            {
                old_parent->children.erase(std::remove(old_parent->children.begin(), old_parent->children.end(), this->shared_from_this()), old_parent->children.end());
            }

            parent = new_parent;
            new_parent->children.push_back(this->shared_from_this());
        }

        bool has_child()
        {
            return !children.empty();
        }

        // checks if this is the current last node in the tree
        bool is_last()
        {
            if (auto p = parent.lock())
            {
                return p->children.back() == this->shared_from_this();
            }
            return false;
        }
    };
}
