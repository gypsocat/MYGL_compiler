#pragma once

#include "../ast-code-visitor.hxx"
#include <memory>

namespace MYGL::GenUtil {
    using namespace Ast;
    class ExprChecker
        : public CodeVisitor,
          public std::enable_shared_from_this<ExprChecker> {
    public:
        using PtrT = std::shared_ptr<ExprChecker>;
        using UnownedPtrT = std::weak_ptr<ExprChecker>;

        using VarMapT = std::unordered_map<Variable::UnownedPtrT, Value::PtrT>;
    public:
        ExprChecker();

        Value::PtrT try_calculate(Expression::UnownedPtrT expr) {
            _constant_table.clear();
            return do_try_calculate(expr);
        }
        Value::PtrT do_try_calculate(Expression::UnownedPtrT expr);

        /** @brief 检查数值表达式是否为常量。
         * 注意，一旦碰到函数表达式、初始化列表等等，那一定会被判false */
        bool value_is_constant(Expression::UnownedPtrT expr) {
            _constant_table.clear();
            return (do_try_calculate(expr) != nullptr);
        }
        /** @brief 检查初始化列表是否为常量 */
        bool list_is_constant(InitList::UnownedPtrT init_list);

        bool visit(UnaryExpr::UnownedPtrT node) override;
        bool visit(BinaryExpr::UnownedPtrT node) override;
        bool visit(CallParam::UnownedPtrT node) override;
        bool visit(CallExpr::UnownedPtrT node) override;
        bool visit(InitList::UnownedPtrT node) override;
        bool visit(IndexExpr::UnownedPtrT node) override;
        bool visit(Identifier::UnownedPtrT node) override;
        bool visit(IntValue::UnownedPtrT node) override;
        bool visit(FloatValue::UnownedPtrT node) override;
        bool visit(StringValue::UnownedPtrT node) override;
        bool visit(AssignExpr::UnownedPtrT node) override;
        bool visit(IfStmt::UnownedPtrT node) override;
        bool visit(WhileStmt::UnownedPtrT node) override;
        bool visit(EmptyStmt::UnownedPtrT node) override;
        bool visit(ReturnStmt::UnownedPtrT node) override;
        bool visit(BreakStmt::UnownedPtrT node) override;
        bool visit(ContinueStmt::UnownedPtrT node) override;
        bool visit(Block::UnownedPtrT node) override;
        bool visit(ExprStmt::UnownedPtrT node) override;
        bool visit(ConstDecl::UnownedPtrT node) override;
        bool visit(VarDecl::UnownedPtrT node) override;
        bool visit(Function::UnownedPtrT node) override;
        bool visit(Variable::UnownedPtrT node) override;
        bool visit(Type::UnownedPtrT node) override;
        bool visit(CompUnit::UnownedPtrT node) override;
        bool visit(FuncParam::UnownedPtrT node) override;
        bool visit(ArrayInfo::UnownedPtrT node) override;
    private:
        Expression::UnownedPtrT _current_expr;
        Value::PtrT             _cur_value;
        VarMapT                 _constant_table;
    }; // class ExprChecker
} // namespace MYGL::IR
