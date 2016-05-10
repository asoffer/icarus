#ifndef ICARUS_UNITY
#include "Scope.h"
#endif

namespace std {
template <> struct less<Dependency::PtrWithTorV> {
  bool operator()(const Dependency::PtrWithTorV &lhs,
                  const Dependency::PtrWithTorV &rhs) const {
    if (lhs.ptr_ != rhs.ptr_) return lhs.ptr_ < rhs.ptr_;
    if (lhs.torv_ != rhs.torv_) return lhs.torv_ < rhs.torv_;
    return false;
  }
};
} // namespace std

namespace debug {
extern bool dependency_graph;

// debug info local to translation unit:
static bool last_torv      = false;
static AST::Node *last_ptr = nullptr;
} // namespace debug

#ifdef DEBUG
#define AT(access) .at((access))
#else
#define AT(access) [(access)]
#endif

namespace Dependency {
static std::map<PtrWithTorV, std::set<PtrWithTorV>> dependencies_ = {};
static std::map<AST::Node *, Flag> already_seen_ = {};

void record(AST::Node *node) {
  node->record_dependencies();
  if (debug::dependency_graph) { Dependency::write_graphviz(); }
}

void type_type(AST::Node *depender, AST::Node *dependee) {
  dependencies_[PtrWithTorV(depender, true)].emplace(dependee, true);
  dependencies_[PtrWithTorV(depender, false)];
}

void type_value(AST::Node *depender, AST::Node *dependee) {
  dependencies_[PtrWithTorV(depender, true)].emplace(dependee, false);
  dependencies_[PtrWithTorV(depender, false)];
}

void value_type(AST::Node *depender, AST::Node *dependee) {
  dependencies_[PtrWithTorV(depender, true)];
  dependencies_[PtrWithTorV(depender, false)].emplace(dependee, true);
}

void value_value(AST::Node *depender, AST::Node *dependee) {
  dependencies_[PtrWithTorV(depender, true)];
  dependencies_[PtrWithTorV(depender, false)].emplace(dependee, false);
}

void add_to_table(AST::Node *depender) {
  dependencies_[PtrWithTorV(depender, true)];
  dependencies_[PtrWithTorV(depender, false)];
}

void mark_as_done(AST::Node *e) {
  add_to_table(e);
  already_seen_[e] = tv_done;
}

void traverse_from(PtrWithTorV pt) {
  std::vector<AST::Node *> node_stack;
  std::vector<bool> torv_stack;
  node_stack.push_back(pt.ptr_);
  torv_stack.push_back(pt.torv_);

  while (!node_stack.empty()) {
    assert(node_stack.size() == torv_stack.size() &&
           "Stacks sizes don't match!");

    auto ptr  = node_stack.back();
    auto torv = torv_stack.back();

    Flag done_flag = (torv ? type_done : val_done);
    if ((already_seen_ AT(ptr) & done_flag) != 0) {
      node_stack.pop_back();
      torv_stack.pop_back();
      continue;
    }

    Flag seen_flag = (torv ? type_seen : val_seen);
    if ((already_seen_ AT(ptr) & seen_flag) != 0) {
      node_stack.pop_back();
      torv_stack.pop_back();

      if (debug::dependency_graph) {
        debug::last_ptr  = ptr;
        debug::last_torv = torv;

        Dependency::write_graphviz();
        std::cin.ignore(1);
      }

      if (torv) {
        // TODO add these via something like lambdas in record_dependencise
        if (ptr->is_unop()) {
          auto unop = static_cast<AST::Unop *>(ptr);
          if (unop->op == Language::Operator::At) {
            auto t = unop->operand->type;
            while (t->is_pointer()) t = static_cast<Pointer *>(t)->pointee;
            if (t->is_pointer()) {
              t = static_cast<Pointer *>(t)->pointee;
              if (t->is_struct()) {
                auto struct_type = static_cast<Structure *>(t);
                PtrWithTorV ptr_with_torv(struct_type->ast_expression, false);
                traverse_from(ptr_with_torv);
              }
            }
          }

        } else if (ptr->is_access()) {
          auto access_ptr = static_cast<AST::Access *>(ptr);
          auto t = access_ptr->operand->type;
          while (t->is_pointer()) t = static_cast<Pointer *>(t)->pointee;
          if (t->is_struct()) {
            auto struct_type = static_cast<Structure *>(t);
            PtrWithTorV ptr_with_torv(struct_type->ast_expression, false);
            traverse_from(ptr_with_torv);
          }

        } else if (ptr->is_binop()) {
          auto binop_ptr = static_cast<AST::Binop *>(ptr);
          if (binop_ptr->op == Language::Operator::Call) {
            if (binop_ptr->lhs->type == Type_) {
              // TODO
              //
              // The code below was to ensure that the LHS was actually a
              // parametric struct. Unfortunately, it can cause dependency
              // cycles. For now, we just assume that calling a type we know
              // that the type is parametric. This probably introduces some bad
              // bugs.

              // PtrWithTorV ptr_with_torv(binop_ptr->lhs, false);
              // traverse_from(ptr_with_torv);
            }
          }
        }

        ptr->verify_types();

      } else {
        if (ptr->is_struct_literal()) {
          // Evaluating a type literal stores the types of its members (but
          // those types may still be opaque.
          ptr->evaluate(ptr->scope_->context);

        } else if (ptr->is_declaration() &&
                   static_cast<AST::Declaration *>(ptr)->type == Type_) {
          ptr->evaluate(ptr->scope_->context);
        }
      }

      // If it's an identifier, push it into the declarations for the
      // appropriate scope, so they can be allocated correctly
      if (torv && ptr->is_identifier()) {
        auto id_ptr = static_cast<AST::Identifier *>(ptr);
        for (auto decl : id_ptr->decls) {
          decl->scope_->ordered_decls_.push_back(decl);
        }
      }

      already_seen_ AT(ptr) = static_cast<Flag>(
          (seen_flag << 1) | already_seen_ AT(ptr)); // Mark it as done

      continue;
    }

    // If you get here, this node is totally new.
    // Mark it as seen
    already_seen_ AT(ptr) =
        static_cast<Flag>(seen_flag | already_seen_ AT(ptr));

    // And follow it's dependencies
    assert(
        (dependencies_.find(PtrWithTorV(ptr, torv)) != dependencies_.end()) &&
        "Not in dependency table");
    for (const auto &dep : dependencies_ AT(PtrWithTorV(ptr, torv))) {
      done_flag = (dep.torv_ ? type_done : val_done);
      seen_flag = (dep.torv_ ? type_seen : val_seen);

      if ((already_seen_ AT(dep.ptr_) & done_flag) != 0) {
        continue;

      } else if ((already_seen_ AT(dep.ptr_) & seen_flag) != 0) {
        error_log.log(dep.ptr_->loc, "Cyclic dependency found.");
        assert(false && "cyclic dep found");

      } else {
        node_stack.push_back(dep.ptr_);
        torv_stack.push_back(dep.torv_);
      }
    }
  }
}

// This is terribly inefficient, because you're rechecking everything.
// TODO do the efficient thing.
void rebuild_already_seen() {
  for (const auto &kv : dependencies_) {
    if (already_seen_.find(kv.first.ptr_) != already_seen_.end()) continue;
    already_seen_[kv.first.ptr_] = unseen;
  }
}

void assign_order() {
  already_seen_.clear();
  for (const auto &kv : dependencies_) {
    already_seen_[kv.first.ptr_] = unseen;
  }

  for (const auto &kv : dependencies_) {
    auto ptr  = kv.first.ptr_;
    auto torv = kv.first.torv_;
    if (torv && ((already_seen_[ptr] & type_seen) != 0)) continue;
    if (!torv && ((already_seen_[ptr] & val_seen) != 0)) continue;
    traverse_from(kv.first);
  }
}

struct GraphVizFile {
  GraphVizFile(const char *filename) : fout_(filename) {
    fout_ << "digraph {\n";
  }

  GraphVizFile &operator<<(const std::string &str) {
    fout_ << str;
    return *this;
  }

  ~GraphVizFile() {
    {
      fout_ << "}";
      fout_.close();
    }
    system("dot -Tpdf dependencies.dot -o dependencies.pdf");
  }

  std::ofstream fout_;
};

std::string graphviz_node(PtrWithTorV x) {
  std::stringstream output;
  output << (x.torv_ ? "  t" : "  v") << x.ptr_;
  return output.str();
}

std::string escape(const std::string &str) {
  std::stringstream output;
  for (size_t i = 0; i < str.size(); ++i) {
    if (str[i] == ' ')
      output << "\\ ";
    else if (str[i] == '\n')
      output << "\\\\n";
    else if (str[i] == '<')
      output << "&lt;";
    else if (str[i] == '>')
      output << "&gt;";
    else if (str[i] == '&')
      output << "&amp;";
    else
      output << str[i];
  }
  return output.str();
}

std::string ShowFlag (Flag f) {
  return std::to_string((f & 12) >> 2) + ", " + std::to_string(f & 3);
}

std::string graphviz_label(PtrWithTorV x) {
  std::stringstream output;
  output << (x.torv_ ? "  t" : "  v") << x.ptr_ << "[label=\"{"
         << escape(x.ptr_->graphviz_label()) << "\t(" << x.ptr_->loc.line_num
         << ")|" << escape(x.ptr_->is_expression()
                               ? static_cast<AST::Expression *>(x.ptr_)
                                     ->type->to_string()
                               : "---");
  if (already_seen_.find(x.ptr_) != already_seen_.end()) {
    output << " [" << ShowFlag(already_seen_ AT(x.ptr_)) << "]";
  }
  output << "}\", fillcolor=\"#88" << (x.torv_ ? "ffaa" : "aaff")
         << "\" color=\""
         << (x.ptr_ == debug::last_ptr && x.torv_ == debug::last_torv ? "red"
                                                                      : "black")
         << "\" penwidth=\"2.0\" shape=\"Mrecord\", style=\"filled\"];\n";
  return output.str();
}

void write_graphviz() {
  GraphVizFile gviz("dependencies.dot");

  for (const auto &node : dependencies_) { gviz << graphviz_label(node.first); }

  for (const auto &dep : dependencies_) {
    for (const auto &d : dep.second) {
      gviz << graphviz_node(dep.first) << " -> " << graphviz_node(d) << ";\n";
    }
  }
}
} // namespace Dep
