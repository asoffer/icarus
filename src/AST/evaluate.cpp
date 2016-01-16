#include "AST.h"

namespace data {
  extern llvm::Value* const_bool(bool b);
  extern llvm::Value* const_char(char c);
  extern llvm::Value* const_int(int n);
  extern llvm::Value* const_real(double d);
  extern llvm::Value* const_uint(size_t n);

}  // namespace data

namespace AST {
  llvm::Value* Expression::llvm_value(Context::Value v) {
    if (type() == Type::get_bool()) {
      return data::const_bool(v.as_bool);

    } else if (type() == Type::get_char()) {
      return data::const_char(v.as_char);

    } else if (type() == Type::get_int()) {
      return data::const_int(v.as_int);

    } else if (type() == Type::get_real()) {
      return data::const_real(v.as_real);

    } else if (type() == Type::get_uint()) {
      return data::const_uint(v.as_uint);
    }

    return nullptr;
  }

  Context::Value Identifier::evaluate(Context& ctx) {
    return ctx.get(shared_from_this());
  }

  Context::Value Unop::evaluate(Context& ctx) {
    if (is_return()) {
      ctx.set_return_value(expr_->evaluate(ctx));

    } else if (is_print()) {
      auto val = expr_->evaluate(ctx);
      if (expr_->type() == Type::get_bool()) {
        std::cout << (val.as_bool ? "true" : "false");

      } else if (expr_->type() == Type::get_char()) {
        std::cout << val.as_char;

      } else if (expr_->type() == Type::get_int()) {
        std::cout << val.as_int;

      } else if (expr_->type() == Type::get_real()) {
        std::cout << val.as_real;

      } else if (expr_->type() == Type::get_type()) {
        std::cout << val.as_type->to_string();

      } else if (expr_->type() == Type::get_uint()) {
        std::cout << val.as_uint;

      } else {
        // TOOD
      }

      std::cout.flush();
      return nullptr;

    } else if (op_ == Language::Operator::Sub) {
      if (type() == Type::get_int()) {
        return Context::Value(-expr_->evaluate(ctx).as_int);

      } else if (type() == Type::get_real()) {
        return Context::Value(-expr_->evaluate(ctx).as_real);
      }

    }

    return nullptr;
  }

  Context::Value ChainOp::evaluate(Context& ctx) { 
    if (exprs_[0]->type() == Type::get_bool()) {
      switch (ops_[0]) {
        case Language::Operator::Xor:
          {
            bool expr_val = false;
            for (auto& expr : exprs_) {
              expr_val = (expr_val != expr->evaluate(ctx).as_bool);
            }
            return Context::Value(expr_val);
          }
        case Language::Operator::And:
          for (auto& expr : exprs_) {
            if (expr->evaluate(ctx).as_bool) return Context::Value(false);
          }
          return Context::Value(true);
        case Language::Operator::Or:
          for (auto& expr : exprs_) {
            if (expr->evaluate(ctx).as_bool) {
              return Context::Value(true);
            }
          }
          return Context::Value(false);
        default:;
      }

      return Context::Value(true);

    } else if (exprs_[0]->type() == Type::get_int()) {
      bool total = true;
      auto last = exprs_[0]->evaluate(ctx);
      for (size_t i = 0; i < ops_.size(); ++i) {
        auto next = exprs_[i + 1]->evaluate(ctx);

        switch (ops_[i]) {
          case Language::Operator::LessThan:
            total &= (last.as_int < next.as_int);
          case Language::Operator::LessEq:
            total &= (last.as_int <= next.as_int);
          case Language::Operator::Equal:
            total &= (last.as_int == next.as_int);
          case Language::Operator::NotEqual:
            total &= (last.as_int != next.as_int);
          case Language::Operator::GreaterThan:
            total &= (last.as_int >= next.as_int);
          case Language::Operator::GreaterEq:
            total &= (last.as_int > next.as_int);
          default:;
        }

        if (!total) {
          return Context::Value(false);
        }

        last = next;
      }

      return Context::Value(true);

    } else if (exprs_[0]->type() == Type::get_type()) {
      auto last = exprs_[0]->evaluate(ctx);
      for (size_t i = 0; i < ops_.size(); ++i) {
        auto next = exprs_[i + 1]->evaluate(ctx);

        if (ops_[i] == Language::Operator::Equal) {
          if (last.as_type != next.as_type) {
            return Context::Value(false);
          }

        } else if (ops_[i] == Language::Operator::NotEqual) {
          if (last.as_type == next.as_type) {
            return Context::Value(false);
          }
        }

        last = next;
      }
      return Context::Value(true);

    } else if (exprs_[0]->type() == Type::get_uint()) {
      bool total = true;
      auto last = exprs_[0]->evaluate(ctx);
      for (size_t i = 0; i < ops_.size(); ++i) {
        auto next = exprs_[i + 1]->evaluate(ctx);

        switch (ops_[i]) {
          case Language::Operator::LessThan:
            total &= (last.as_int < next.as_int);
          case Language::Operator::LessEq:
            total &= (last.as_int <= next.as_int);
          case Language::Operator::Equal:
            total &= (last.as_int == next.as_int);
          case Language::Operator::NotEqual:
            total &= (last.as_int != next.as_int);
          case Language::Operator::GreaterThan:
            total &= (last.as_int >= next.as_int);
          case Language::Operator::GreaterEq:
            total &= (last.as_int > next.as_int);
          default:;
        }

        if (!total) {
          return Context::Value(false);
        }
        last = next;
      }

      return Context::Value(true);

    } else {
      return nullptr;
    }
  }

  Context::Value ArrayType::evaluate(Context&)       { return nullptr; }
  Context::Value ArrayLiteral::evaluate(Context&)    { return nullptr; }

  Context::Value Terminal::evaluate(Context& ctx) {
    if (type() == Type::get_bool()) {
      if (token() == "true") {
        return Context::Value(true);

      } else if (token() == "false") {
        return Context::Value(false);

      } else {
        std::cerr << "FATAL: BOOL LITERAL?" << std::endl;
        return nullptr;
      }

    } else if (type() == Type::get_char()) {
      return Context::Value(token()[0]);

    } else if (type() == Type::get_int()) {
      return Context::Value(std::stoi(token()));

    } else if (type() == Type::get_real()) {
      return Context::Value(std::stod(token()));

    } else if (type() == Type::get_uint()) {
      return Context::Value(std::stoul(token()));

    } else if (type() == Type::get_type()) {
      return Context::Value(interpret_as_type());

    } else {
      // TODO
      return nullptr;
    }
  }

  Context::Value FunctionLiteral::evaluate(Context& ctx) {
    return statements_->evaluate(ctx);
  }

  Context::Value Case::evaluate(Context& ctx) {
    for (size_t i = 0; i < pairs_->kv_pairs_.size() - 1; ++i) {
      auto pair = pairs_->kv_pairs_[i];

      if (pair.first->evaluate(ctx).as_bool) {
        return pair.second->evaluate(ctx);
      }
    }
    return pairs_->kv_pairs_.back().second->evaluate(ctx);
  }

  Context::Value Assignment::evaluate(Context&)      { return nullptr; }
  Context::Value Declaration::evaluate(Context&)     { return nullptr; }
  Context::Value TypeLiteral::evaluate(Context&)     { return nullptr; }
  Context::Value EnumLiteral::evaluate(Context&)     { return nullptr; }

  Context::Value Binop::evaluate(Context& ctx) {
    if (op_ == Language::Operator::Call) {
      if (lhs_->is_identifier()) {
        auto expr_ptr = ctx.get(std::static_pointer_cast<Identifier>(lhs_)).as_expr;
        // TODO must lhs_ be a function?
        auto fn_ptr = static_cast<FunctionLiteral*>(expr_ptr);
        Context fn_ctx = Context::GlobalContext.spawn();

        // Populate the function context with arguments
        for (const auto& arg : fn_ptr->inputs_) {
          fn_ctx.bind(rhs_->evaluate(ctx), arg->declared_identifier());
        }


        auto x = fn_ptr->evaluate(fn_ctx);
        return x;
      }
    }

    return nullptr;
  }

  Context::Value KVPairList::evaluate(Context&)      { return nullptr; }
  Context::Value Statements::evaluate(Context& ctx) {
    for (auto& stmt : statements_) {
      stmt->evaluate(ctx);
      if (ctx.has_return()) {
        return ctx.return_value();
      }
    }

    return nullptr;
  }

  Context::Value Conditional::evaluate(Context& ctx) {
    for (size_t i = 0; i < conds_.size(); ++i) {
      if (conds_[i]->evaluate(ctx).as_bool) {
        Context cond_ctx(&ctx);
        statements_[i]->evaluate(cond_ctx);
        if (cond_ctx.has_return()) {
          return cond_ctx.return_value();
        }
      }
    }

    if (has_else()) {
        Context cond_ctx(&ctx);
        statements_.back()->evaluate(cond_ctx);
        if (cond_ctx.has_return()) {
          return cond_ctx.return_value();
        }
    }

    return nullptr;
  }

  Context::Value Break::evaluate(Context&)           { return nullptr; }
  Context::Value While::evaluate(Context&)           { return nullptr; }
}  // namespace AST
