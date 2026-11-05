#include "error_builtin.hpp"

#include <algorithm>
#include <functional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../lexer/lexer.hpp"
#include "parser.hpp"

namespace occult {
    namespace {
        // Error runtime, parsed in alongside programs that raise or catch.
        constexpr const char* runtime_linux = R"(
module __occult_rt {
    fn write_err(i64 buf, i64 n) asm(x86_64) {
        mov rdx, rsi
        mov rsi, rdi
        mov edi, 2
        mov eax, 1
        syscall
        ret
    }

    fn quit(i64 code) asm(x86_64) {
        mov eax, 231
        syscall
        ret
    }

    extern fn __errno_location() i64;

    fn errno() i64 {
        return $(i32*(__errno_location())) & 0xFFFFFFFF;
    }
)";

        constexpr const char* runtime_win64 = R"(
module msvcrt link "msvcrt" {
    extern fn _errno() i64;
}

module __occult_rt link "kernel32" {
    extern fn GetStdHandle(i64 which) i64;
    extern fn WriteFile(i64 file, i64 buf, i64 n, i64 written, i64 overlapped) i64;
    extern fn ExitProcess(i64 code) i64;

    fn write_err(i64 buf, i64 n) {
        i64 written = 0;
        WriteFile(GetStdHandle(0 - 12), buf, n, @written, 0);
    }

    fn quit(i64 code) {
        ExitProcess(code);
    }

    fn errno() i64 {
        return $(i32*(msvcrt::_errno())) & 0xFFFFFFFF;
    }
)";

        constexpr const char* runtime_common = R"(
    fn length(i64 s) i64 {
        if s == 0 { return 0; }
        i64 n = 0;
        while $(u8*(s + n)) != 0 { n = n + 1; }
        return n;
    }

    fn raise(i64 type_id, i64 message, i64 where) {
        __occult_err_active = 1;
        __occult_err_type_id = type_id;
        __occult_err_message = message;
        __occult_err_where = where;
    }

    fn clear() {
        __occult_err_active = 0;
        __occult_err_type_id = 0;
        __occult_err_message = 0;
        __occult_err_where = 0;
    }

    fn uncaught() {
        write_err("error: ", 7);
        if __occult_err_message != 0 {
            write_err(__occult_err_message, length(__occult_err_message));
        }
        if __occult_err_where != 0 {
            write_err(__occult_err_where, length(__occult_err_where));
        }
        quit(1);
    }
}

// payload buffer: up to 8 slots, one per field.
i64 __occult_err_active = 0;
i64 __occult_err_type_id = 0;
i64 __occult_err_message = 0;
i64 __occult_err_where = 0;
i64 __occult_err_payload_0 = 0;
i64 __occult_err_payload_1 = 0;
i64 __occult_err_payload_2 = 0;
i64 __occult_err_payload_3 = 0;
i64 __occult_err_payload_4 = 0;
i64 __occult_err_payload_5 = 0;
i64 __occult_err_payload_6 = 0;
i64 __occult_err_payload_7 = 0;
)";

        std::string bare(const std::string& name) {
            const auto at = name.rfind("::");
            return at == std::string::npos ? name : name.substr(at + 2);
        }

        // Build a __occult_rt::raise(type_id, message, where) call.
        std::unique_ptr<cst> make_raise_call(std::int64_t type_id, std::vector<std::unique_ptr<cst>>& message_nodes, const std::string& in_function, const std::string& origin) {
            auto call = cst::new_node<cst_functioncall>("start_call");
            call->origin = origin;
            call->add_child(cst::new_node<cst_identifier>("__occult_rt::raise"));
            auto tid_arg = cst::new_node<cst_functionarg>();
            tid_arg->add_child(cst::new_node<cst_numberliteral>(std::to_string(type_id)));
            call->add_child(std::move(tid_arg));
            auto msg_arg = cst::new_node<cst_functionarg>();
            for (auto& n : message_nodes) {
                msg_arg->add_child(std::move(n));
            }
            call->add_child(std::move(msg_arg));
            auto where = cst::new_node<cst_functionarg>();
            where->add_child(cst::new_node<cst_stringliteral>("\n  at " + in_function + " (" + origin + ")\n"));
            call->add_child(std::move(where));
            call->add_child(cst::new_node<cst_functioncall>("end_call"));
            return call;
        }

        // Typed raise: write field initialisers to payload slots, then raise.
        void expand_typed_raise(std::vector<std::unique_ptr<cst>>& out, cst* err_stmt,
                                std::int64_t type_id, const std::string& in_function,
                                const std::vector<std::string>& field_names) {
            const std::string origin = err_stmt->origin;
            const std::string type_name = err_stmt->content;
            auto& kids = err_stmt->get_children();
            std::unordered_map<std::string, cst*> inits;
            for (std::size_t i = 1; i < kids.size(); ++i) {
                if (kids[i]->get_type() == cst_type::functionargument && !kids[i]->get_children().empty() &&
                    kids[i]->get_children().front()->get_type() == cst_type::identifier) {
                    const std::string& fname = kids[i]->get_children().front()->content;
                    inits[fname] = kids[i].get();
                }
            }
            for (const auto& [fname, farg] : inits) {
                auto it = std::find(field_names.begin(), field_names.end(), fname);
                if (it == field_names.end()) {
                    throw std::runtime_error("field \"" + fname + "\" is not in error type \"" + type_name + "\", at " + origin);
                }
                const auto idx = static_cast<std::size_t>(it - field_names.begin());
                if (idx >= 8) {
                    throw std::runtime_error("error type \"" + type_name + "\" has more than eight fields; this release supports eight at most, at " + origin);
                }
                // emit: __occult_err_payload_<n> = <expr>;
                auto decl = cst::new_node<cst_int64>();
                decl->add_child(cst::new_node<cst_identifier>("__occult_err_payload_" + std::to_string(idx)));
                auto assn = cst::new_node<cst_assignment>();
                for (auto& ec : farg->get_children()) {
                    if (ec->get_type() == cst_type::identifier &&
                        ec.get() == farg->get_children().front().get()) {
                        continue;   // skip the field name
                    }
                    assn->add_child(std::move(ec));
                }
                decl->add_child(std::move(assn));
                out.push_back(std::move(decl));
            }
            // the raise call itself: the message is the type's name as a C string
            auto name_arg = cst::new_node<cst_stringliteral>(type_name);
            std::vector<std::unique_ptr<cst>> message_nodes;
            message_nodes.push_back(std::move(name_arg));
            out.push_back(make_raise_call(type_id, message_nodes, in_function, origin));
        }

        struct error_type_info {
            std::int64_t id;
            std::vector<std::string> field_names;
        };

        // Returns name -> info for every struct marked `error`.  Walks the
        // whole tree, including inside modules.  IDs are assigned 1..N in
        // Build the id table: 0 is untyped, user types from 1.
        std::unordered_map<std::string, error_type_info> register_error_types(cst* root) {
            std::unordered_map<std::string, error_type_info> types;
            std::int64_t next_id = 1;
            std::function<void(cst*)> scan = [&](cst* n) {
                if (n->get_type() == cst_type::structure && !n->get_children().empty()) {
                    bool is_error = false;
                    std::vector<std::string> fields;
                    for (auto& k : n->get_children()) {
                        if (k->get_type() == cst_type::struct_is_error) {
                            is_error = true;
                        }
                        else if (!k->get_children().empty() && k->get_children().front()->get_type() == cst_type::identifier &&
                                 (k->get_type() == cst_type::int64_datatype || k->get_type() == cst_type::string_datatype)) {
                            fields.push_back(k->get_children().front()->content);
                        }
                    }
                    if (is_error) {
                        const std::string& name = n->content;
                        if (!types.contains(name)) {
                            types[name] = {next_id++, std::move(fields)};
                            n->type_id = types[name].id;
                        }
                    }
                }
                for (auto& k : n->get_children()) {
                    scan(k.get());
                }
            };
            scan(root);
            return types;
        }

        // Prepend "TypeName e; e.f = __occult_err_payload_N; ..." to the catch body.
        void inject_catch_prelude(cst* clause, const std::string& type_name,
                                  const std::vector<std::string>& fields) {
            cst* body = nullptr;
            for (auto& k : clause->get_children()) {
                if (k->get_type() == cst_type::block) {
                    body = k.get();
                    break;
                }
            }
            if (body == nullptr) return;
            auto& children = body->get_children();
            std::vector<std::unique_ptr<cst>> prelude;

            auto decl = cst::new_node<cst_struct>();
            decl->content = type_name;
            decl->add_child(cst::new_node<cst_identifier>(clause->content));
            prelude.push_back(std::move(decl));

            for (std::size_t i = 0; i < fields.size() && i < 8; ++i) {
                auto m = cst::new_node<cst_memberaccess>();
                m->add_child(cst::new_node<cst_identifier>(clause->content));
                m->add_child(cst::new_node<cst_identifier>(fields[i]));
                auto assn = cst::new_node<cst_assignment>();
                assn->add_child(cst::new_node<cst_identifier>("__occult_err_payload_" + std::to_string(i)));
                m->add_child(std::move(assn));
                prelude.push_back(std::move(m));
            }

            for (auto it = prelude.rbegin(); it != prelude.rend(); ++it) {
                children.insert(children.begin(), std::move(*it));
            }
        }

        void stamp_catch_types(cst* n, const std::unordered_map<std::string, error_type_info>& types) {
            if (n->get_type() == cst_type::catchclause) {
                if (n->library.empty()) {
                    n->type_id = 0;
                }
                else {
                    auto it = types.find(n->library);
                    if (it == types.end()) {
                        throw std::runtime_error("catch names unknown error type \"" + n->library + "\"");
                    }
                    n->type_id = it->second.id;
                    inject_catch_prelude(n, n->library, it->second.field_names);
                }
            }
            for (auto& k : n->get_children()) {
                stamp_catch_types(k.get(), types);
            }
        }

        struct catch_binding {
            std::string name;
            std::int64_t type_id;
            std::string type_name;
            std::vector<std::string> field_names;
        };

        void rewrite(cst* node, const std::string& in_function, std::size_t& count,
                     const std::unordered_map<std::string, error_type_info>& types,
                     std::vector<catch_binding>& binders) {
            auto& kids = node->get_children();
            for (std::size_t i = 0; i < kids.size(); ++i) {
                auto& ch = kids[i];
                std::string here = in_function;
                if (ch->get_type() == cst_type::function && !ch->get_children().empty()) {
                    here = ch->get_children().front()->content;
                }

                if (ch->get_type() == cst_type::errorstmt) {
                    if (ch->get_children().empty()) {
                        throw std::runtime_error("error has no message, at " + ch->origin);
                    }
                    if (!ch->content.empty()) {
                        // first: does the identifier name a declared error type?
                        auto it = types.find(ch->content);
                        if (it != types.end()) {
                            std::vector<std::unique_ptr<cst>> replacement;
                            expand_typed_raise(replacement, ch.get(), it->second.id, here, it->second.field_names);
                            kids.erase(kids.begin() + static_cast<std::ptrdiff_t>(i));
                            for (auto& r : replacement) {
                                kids.insert(kids.begin() + static_cast<std::ptrdiff_t>(i++), std::move(r));
                            }
                            --i;
                            count += replacement.size();
                            continue;
                        }
                        auto binder = std::find_if(binders.begin(), binders.end(),
                            [&](const catch_binding& b) { return b.name == ch->content; });
                        if (binder != binders.end()) {
                            std::vector<std::unique_ptr<cst>> replacement;
                            // restore payload fields from the catch binder for a typed re-raise
                            for (std::size_t fi = 0; fi < binder->field_names.size() && fi < 8; ++fi) {
                                auto decl = cst::new_node<cst_int64>();
                                decl->add_child(cst::new_node<cst_identifier>("__occult_err_payload_" + std::to_string(fi)));
                                auto assn = cst::new_node<cst_assignment>();
                                auto read = cst::new_node<cst_memberaccess>();
                                read->add_child(cst::new_node<cst_identifier>(binder->name));
                                read->add_child(cst::new_node<cst_identifier>(binder->field_names[fi]));
                                assn->add_child(std::move(read));
                                decl->add_child(std::move(assn));
                                replacement.push_back(std::move(decl));
                            }
                            std::vector<std::unique_ptr<cst>> message_nodes;
                            if (binder->type_id == 0) {
                                message_nodes.push_back(cst::new_node<cst_identifier>(binder->name));
                            }
                            else {
                                message_nodes.push_back(cst::new_node<cst_stringliteral>(binder->type_name));
                            }
                            replacement.push_back(make_raise_call(binder->type_id, message_nodes, here, ch->origin));
                            kids.erase(kids.begin() + static_cast<std::ptrdiff_t>(i));
                            for (auto& r : replacement) {
                                kids.insert(kids.begin() + static_cast<std::ptrdiff_t>(i++), std::move(r));
                            }
                            --i;
                            count += replacement.size();
                            continue;
                        }
                        throw std::runtime_error("error type \"" + ch->content + "\" is not declared, at " + ch->origin);
                    }
                    auto& msg_nodes = ch->get_children();
                    kids[i] = make_raise_call(0, msg_nodes, here, ch->origin);
                    ++count;
                    continue;
                }

                if (ch->get_type() == cst_type::catchclause) {
                    catch_binding cb;
                    cb.name = ch->content;
                    cb.type_id = ch->type_id;
                    cb.type_name = ch->library; // parser stashed the type here
                    if (!cb.type_name.empty()) {
                        if (auto ti = types.find(cb.type_name); ti != types.end()) {
                            cb.field_names = ti->second.field_names;
                        }
                    }
                    binders.push_back(std::move(cb));
                    rewrite(ch.get(), here, count, types, binders);
                    binders.pop_back();
                    continue;
                }

                rewrite(ch.get(), here, count, types, binders);
            }
        }

        void rewrite(cst* node, const std::string& in_function, std::size_t& count,
                     const std::unordered_map<std::string, error_type_info>& types) {
            std::vector<catch_binding> binders;
            rewrite(node, in_function, count, types, binders);
        }

        bool uses_error(cst* root) {
            bool found = false;
            std::function<void(cst*)> scan = [&](cst* n) {
                if (found) return;
                if (n->get_type() == cst_type::trystmt) {
                    found = true;
                    return;
                }
                for (auto& k : n->get_children()) {
                    scan(k.get());
                }
            };
            scan(root);
            return found;
        }

        // Fixpoint over the call graph: which functions can reach a raise.
        std::unordered_set<std::string> fallible_functions(cst* root) {
            std::unordered_map<std::string, cst*> defs;
            std::unordered_set<std::string> raises_directly;
            std::unordered_set<std::string> externs;
            std::function<void(cst*, std::string)> walk = [&](cst* n, std::string in) {
                if (n->get_type() == cst_type::function && !n->get_children().empty()) {
                    in = n->get_children().front()->content;
                    defs[in] = n;
                    for (auto& k : n->get_children()) {
                        if (k->get_type() == cst_type::func_is_external) {
                            externs.insert(in);
                            break;
                        }
                    }
                }
                if (!in.empty() && n->get_type() == cst_type::functioncall && n->content == "start_call" &&
                    !n->get_children().empty() && n->get_children().front()->get_type() == cst_type::identifier) {
                    const std::string& callee = n->get_children().front()->content;
                    if (callee == "__occult_rt::raise") {
                        raises_directly.insert(in);
                    }
                }
                for (auto& k : n->get_children()) {
                    walk(k.get(), in);
                }
            };
            walk(root, "");

            std::unordered_set<std::string> fallible = raises_directly;
            fallible.insert("__occult_rt::raise");
            bool changed = true;
            while (changed) {
                changed = false;
                for (const auto& [name, node] : defs) {
                    if (fallible.contains(name)) continue;
                    bool reaches = false;
                    std::function<void(cst*)> scan = [&](cst* n) {
                        if (reaches) return;
                        if (n->get_type() == cst_type::functioncall && n->content == "start_call" &&
                            !n->get_children().empty() && n->get_children().front()->get_type() == cst_type::identifier) {
                            const std::string& callee = n->get_children().front()->content;
                            if (fallible.contains(callee) && !externs.contains(callee)) {
                                reaches = true;
                                return;
                            }
                        }
                        for (auto& k : n->get_children()) {
                            scan(k.get());
                        }
                    };
                    scan(node);
                    if (reaches) {
                        fallible.insert(name);
                        changed = true;
                    }
                }
            }
            return fallible;
        }

        // Walk each function, point every fallible call at its handler.
        void assign_handlers(cst* root, const std::unordered_set<std::string>& fallible, std::unordered_set<std::string>& externs_out) {
            std::function<void(cst*)> collect_externs = [&](cst* n) {
                if (n->get_type() == cst_type::function && !n->get_children().empty()) {
                    for (auto& k : n->get_children()) {
                        if (k->get_type() == cst_type::func_is_external) {
                            externs_out.insert(n->get_children().front()->content);
                            break;
                        }
                    }
                }
                for (auto& k : n->get_children()) {
                    collect_externs(k.get());
                }
            };
            collect_externs(root);

            std::size_t try_counter = 0;
            std::function<void(cst*, std::vector<std::string>&)> visit = [&](cst* n, std::vector<std::string>& stack) {
                if (n->get_type() == cst_type::trystmt) {
                    const auto id = "__oc_try_" + std::to_string(try_counter++);
                    n->content = id;
                    auto& kids = n->get_children();
                    stack.push_back(id + "_dispatch");
                    if (!kids.empty()) {
                        visit(kids[0].get(), stack);
                    }
                    stack.pop_back();
                    stack.push_back(id + "_finally");
                    if (kids.size() >= 2) {
                        visit(kids[1].get(), stack);
                    }
                    stack.pop_back();
                    for (std::size_t i = 2; i < kids.size(); ++i) {
                        visit(kids[i].get(), stack);
                    }
                    return;
                }
                if (n->get_type() == cst_type::functioncall && n->content == "start_call" &&
                    !n->get_children().empty() && n->get_children().front()->get_type() == cst_type::identifier) {
                    const std::string& callee = n->get_children().front()->content;
                    const bool indirect = false;
                    if ((fallible.contains(callee) && !externs_out.contains(callee)) || indirect) {
                        n->handler = stack.empty() ? std::string("__oc_prop") : stack.back();
                    }
                }
                for (auto& k : n->get_children()) {
                    visit(k.get(), stack);
                }
            };
            std::vector<std::string> stack;
            visit(root, stack);
        }
    } // namespace

    void lower_error_builtin(cst* root, bool target_win64) {
        const auto types = register_error_types(root);
        stamp_catch_types(root, types);

        std::size_t raises = 0;
        rewrite(root, "<top level>", raises, types);

        if (raises == 0 && !uses_error(root)) {
            return;
        }

        const auto fallible = fallible_functions(root);

        std::unordered_set<std::string> externs;
        assign_handlers(root, fallible, externs);

        const std::string src = std::string(target_win64 ? runtime_win64 : runtime_linux) + runtime_common;
        lexer l(src);
        const auto tokens = l.analyze();
        parser p(tokens, "<occult runtime>");
        auto runtime = p.parse();
        if (p.get_state() == parser::state::failed) {
            throw std::runtime_error("the runtime for error() did not parse");
        }
        for (auto& ch : runtime->get_children()) {
            root->add_child(std::move(ch));
        }
    }
} // namespace occult
