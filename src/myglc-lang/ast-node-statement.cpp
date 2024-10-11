/** @file ast-node-statement.cpp
 * @brief 语句结点的实现 */

#include "myglc-lang/ast-code-visitor.hxx"
#include "myglc-lang/ast-node-decl.hxx"
#include "myglc-lang/ast-node.hxx"
#include "myglc-lang/ast-exception.hxx"
#include "myglc-lang/ast-scope.hxx"
#include <memory>
#include <iostream>
#include <format>
#include <string_view>
#include <utility>


#define _M_SET_STMT_EX(child, expr, true_return, false_return) {\
    if ((expr).get() == (child).get()) {\
        true_return;\
    } else if ((expr) != nullptr) {\
        (expr)->set_parent(this);\
        child = (expr);\
        true_return;\
    }\
    false_return;\
}
#define _M_SET_STMT(child, expr) \
    _M_SET_STMT_EX(child, expr, {}, {})
#define _M_SET_STMT_BOOL(child, expr) \
    _M_SET_STMT_EX(child, expr, {return true;}, {return false;})

#define _M_MOVE_STMT_EX(child, rrexpr, true_return, false_return) {\
    if ((rrexpr).get() == (child).get()) {\
        true_return;\
    } else if ((rrexpr) != nullptr) {\
        (rrexpr)->set_parent(this);\
        child = std::move(rrexpr);\
        {true_return};\
    }\
    {false_return};\
}
#define _M_MOVE_STMT(child, rrexpr) \
    _M_MOVE_STMT_EX(child, rrexpr, {}, {})
#define _M_MOVE_STMT_BOOL(child, rrexpr) \
    _M_MOVE_STMT_EX(child, rrexpr, {return true;}, {return false;})

#define _M_LIST_ADD_STMT_EX(child, expr, op, true_return, false_return) {\
    if ((expr) == nullptr) { \
        false_return;\
    } else {\
        (expr)->set_parent(this);\
        child.op(expr);\
        {true_return};\
    }\
}
#define _M_LIST_ADD_STMT(child, expr, op) \
    _M_LIST_ADD_STMT_EX(child, expr, op, {}, {})
#define _M_LIST_ADD_STMT_BOOL(child, expr, op) \
    _M_LIST_ADD_STMT_EX(child, expr, op, {return true;}, {return false;})

#define _M_LIST_MOVE_STMT_EX(child, expr, op, true_return, false_return) {\
    if ((expr) == nullptr) { \
        false_return;\
    } else {\
        (expr)->set_parent(this);\
        child.op(std::move(expr));\
        {true_return};\
    }\
}
#define _M_LIST_MOVE_STMT(child, expr, op) \
    _M_LIST_MOVE_STMT_EX(child, expr, op, {}, {})
#define _M_LIST_MOVE_STMT_BOOL(child, expr, op) \
    _M_LIST_MOVE_STMT_EX(child, expr, op, {return true;}, {return false;})

#define _M_FAIL_IF_SCOPE_NULL(level, msg) \
    if (_scope == nullptr) {\
        throw NullException(this, level, msg);\
    }

namespace MYGL::Ast {
/* @class Statement */
/* end class Statement */

/* @class Block */
    bool Block::accept(CodeVisitor *visitor) {
        _M_FAIL_IF_SCOPE_NULL(ErrorLevel::WARNING, "Unregistered block");
        return visitor->visit(this);
    }
    bool Block::accept_children(CodeVisitor *visitor)
    {
        for (auto &i: _statements) {
            if (i->accept(visitor) == false)
                return false;
        }
        return true;
    }
    bool Block::register_this(Scope *owner_scope)
    {
        set_scope(owner_scope);
        _scope_self = std::make_unique<Scope>(owner_scope, this);
        /* Block is a scope container */
        for (auto &i : _statements) {
            if (i->register_this(_scope_self.get()) == false)
                return false;
        }
        return true;
    }

    bool Block::is_function_root() const
    {
        if (_parent_node == nullptr)
            return false;
        return _parent_node->node_type() == NodeType::FUNC_DEF;
    }
/* end class Block */

/* @class EmptyStmt */
/* end class EmptyStmt */

/* @class IfStmt */
    IfStmt::IfStmt(SourceRange const &range,
                   Expression::PtrT &&condition,
                   Statement::PtrT &&true_stmt,
                   Statement::PtrT &&false_stmt,
                   Node *parent, Scope *parent_scope)
        : Statement(range, NodeType::IF_STMT, parent, parent_scope),
          _condition(std::move(condition)),
          _true_stmt(std::move(true_stmt)),
          _false_stmt(std::move(false_stmt)) {
        _condition->set_parent(this);
        _true_stmt->set_parent(this);
        if (false_stmt != nullptr)
            _false_stmt->set_parent(this);
    }

    bool IfStmt::accept(CodeVisitor *visitor) {
        _M_FAIL_IF_SCOPE_NULL(ErrorLevel::WARNING, "unregistered If statement");
        return visitor->visit(this);
    }
    bool IfStmt::accept_children(CodeVisitor *visitor) {
        if (_condition->accept(visitor) == false)
            return false;
        if (_true_stmt->accept(visitor) == false)
            return false;
        if (_false_stmt != nullptr && _false_stmt->accept(visitor) == false)
            return false;
        return true;
    }
    bool IfStmt::register_this(Scope::UnownedPtrT owner_scope)
    {
        if (owner_scope == nullptr)
            return false;
        set_scope(owner_scope);
        if (_condition->register_this(owner_scope) == false)
            return false;
        if (_true_stmt->register_this(owner_scope) == false)
            return false;
        if (_false_stmt != nullptr && (_false_stmt->register_this(owner_scope) == false))
            return false;
        return true;
    }

    void IfStmt::set_true_stmt(Statement::PtrT const &true_stmt)
        _M_SET_STMT(_true_stmt, true_stmt)
    void IfStmt::set_true_stmt(Statement::PtrT &&true_stmt)
        _M_MOVE_STMT(_true_stmt, true_stmt)
    void IfStmt::set_false_stmt(Statement::PtrT const &false_stmt)
        _M_SET_STMT(_false_stmt, false_stmt)
    void IfStmt::set_false_stmt(Statement::PtrT &&false_stmt)
        _M_MOVE_STMT(_false_stmt, false_stmt)
    void IfStmt::set_condition(Expression::PtrT const &cond)
        _M_SET_STMT(_condition, cond)
    void IfStmt::set_condition(Expression::PtrT &&cond)
        _M_MOVE_STMT(_condition, cond)
/* end class IfStmt */

/* @class WhileStmt*/
    WhileStmt::WhileStmt(SourceRange const &range,
                      Expression::PtrT &&condition,
                      Statement::PtrT &&true_stmt,
                      Node *parent, Scope *parent_scope)
        : Statement(range, NodeType::WHILE_STMT, parent, parent_scope),
          _condition(std::move(condition)),
          _true_stmt(std::move(true_stmt)) {
        _condition->set_parent(this);
        _true_stmt->set_parent(this);
    }

    bool WhileStmt::accept(CodeVisitor *visitor) {
        _M_FAIL_IF_SCOPE_NULL(ErrorLevel::WARNING, "While statement not registered");
        return visitor->visit(this);
    }
    bool WhileStmt::accept_children(CodeVisitor *visitor) {
        return _condition->accept(visitor) && _true_stmt->accept(visitor);
    }
    bool WhileStmt::register_this(Scope *owner) {
        if (owner == nullptr)
            return false;
        set_scope(owner);
        return _condition->register_this(owner) &&
               _true_stmt->register_this(owner);
    }

    void WhileStmt::set_true_stmt(Statement::PtrT const &true_stmt)
        _M_SET_STMT(_true_stmt, true_stmt)
    void WhileStmt::set_true_stmt(Statement::PtrT &&rrtrue_stmt)
        _M_MOVE_STMT(_true_stmt, rrtrue_stmt)
    void WhileStmt::set_condition(Expression::PtrT const &condition)
        _M_SET_STMT(_condition, condition)
    void WhileStmt::set_condition(Expression::PtrT &&condition)
        _M_MOVE_STMT(_condition, condition)
/* end class WhileStmt*/

/* @class ReturnStmt */
    ReturnStmt::ReturnStmt(SourceRange const &range,
                           Expression::PtrT &&return_expression)
        : Statement(range, NodeType::RETURN_STMT),
          _expression(std::move(return_expression)) {
        _expression->set_parent(this);
    }

    bool ReturnStmt::accept(CodeVisitor *visitor) {
        _M_FAIL_IF_SCOPE_NULL(ErrorLevel::WARNING, "Unregistered return statement");
        return visitor->visit(this);
    }
    bool ReturnStmt::accept_children(CodeVisitor *visitor)
    {
        if (_expression != nullptr)
            return _expression->accept(visitor);
        return false;
    }
    bool ReturnStmt::register_this(Scope *owner)
    {
        set_scope(owner);
        if (_expression == nullptr)
            return true;
        return _expression->register_this(owner);
    }

    void ReturnStmt::set_expression(Expression::PtrT const &expr)
        _M_SET_STMT(_expression, expr)
    void ReturnStmt::set_expression(Expression::PtrT &&expr)
        _M_MOVE_STMT(_expression, expr)
/* end class ReturStmt */

/* @class BreakStmt */
    BreakStmt::BreakStmt(SourceRange const &range)
        : Statement(range, NodeType::BREAK_STMT) {
        _control_node = nullptr;
    }

    bool BreakStmt::accept(CodeVisitor *visitor) {
        if (_scope == nullptr)
            throw NullException(this, ErrorLevel::WARNING, "Unregistered break statement");
        bool ret = visitor->visit(this);
        return _control_node != nullptr && ret;
    }
    bool BreakStmt::accept_children(CodeVisitor *visitor) {
        return true;
    }
    bool BreakStmt::register_this(Scope *owner) {
        if (owner == nullptr || _parent_node == nullptr)
            return false;
        Statement *parents = dynamic_cast<Statement*>(_parent_node);
        while (parents != nullptr) {
            if (parents->node_type() == NodeType::IF_STMT ||
                parents->node_type() == NodeType::WHILE_STMT)
                _control_node = parents;
            parents = dynamic_cast<Statement*>(parents->parent());
        }
        set_scope(owner);
        return true;
    }
    bool BreakStmt::register_control_node()
    {
        Statement *parents = dynamic_cast<Statement*>(_parent_node);
        while (parents != nullptr) {
            if (parents->node_type() == NodeType::IF_STMT ||
                parents->node_type() == NodeType::WHILE_STMT) {
                _control_node = parents;
                break;
            }
            parents = dynamic_cast<Statement*>(parents->parent());
        }
        return (parents != nullptr);
    }
/* end class BreakStmt */
/* @class ContinueStmt */
    ContinueStmt::ContinueStmt(SourceRange const &range)
        : Statement(range, NodeType::CONTINUE_STMT) {
        _control_node = nullptr;
    }

    bool ContinueStmt::accept(CodeVisitor *visitor) {
        _M_FAIL_IF_SCOPE_NULL(ErrorLevel::WARNING, "Unregistered continue statement");
        bool ret = visitor->visit(this);
        return _control_node != nullptr && ret;
    }
    bool ContinueStmt::accept_children(CodeVisitor *visitor) {
        return visitor->visit(this);
    }
    bool ContinueStmt::register_this(Scope *owner)
    {
        if (owner == nullptr)
            return false;
        set_scope(owner);
        return register_control_node();
    }
    bool ContinueStmt::register_control_node()
    {
        Statement *parents = dynamic_cast<Statement*>(_parent_node);
        while (parents != nullptr) {
            if (parents->node_type() == NodeType::WHILE_STMT) {
                _control_node = dynamic_cast<WhileStmt*>(parents);
                return true;
            }
            parents = dynamic_cast<Statement*>(_parent_node);
        }
        return false;
    }
/* end class ContinueStmt */

/* @class ExprStmt */
    ExprStmt::ExprStmt(SourceRange const &range,
                     Expression::PtrT &&expression,
                     Node *parent, Scope *owner_scope)
        : Statement(range, NodeType::CONTINUE_STMT, parent, owner_scope),
          _expression(std::move(expression)) {
        _expression->set_parent(this);
    }

    bool ExprStmt::accept(CodeVisitor *visitor) {
        _M_FAIL_IF_SCOPE_NULL(ErrorLevel::WARNING, "Unregistered expression statement");
        return visitor->visit(this);
    }
    bool ExprStmt::accept_children(CodeVisitor *visitor)
    {
        return _expression->accept(visitor);
    }
    bool ExprStmt::register_this(Scope *owner)
    {
        if (owner == nullptr)
            return false;
        set_scope(owner);
        return _expression->register_this(owner);
    }

    void ExprStmt::set_expression(Expression::PtrT const &value)
        _M_SET_STMT(_expression, value)
    void ExprStmt::set_expression(Expression::PtrT &&value)
        _M_MOVE_STMT(_expression, value)
/* end class ExprStmt */

/* @class Declaration */
    Declaration::Declaration(SourceRange const &range,
                             Type *base_type, bool is_constant,
                             Node *parent, Scope *owner_scope)
        : Statement(range, NodeType::DECL, parent, owner_scope),
          _base_type(base_type), _is_constant(is_constant){}
    
    bool Declaration::append(Variable::PtrT const &var)
    {
        if (var == nullptr)
            throw NullException(this, ErrorLevel::CRITICAL, "appended null variable");
        var->set_parent(this);
        _variables.push_back(var);
        return true;
    }
    bool Declaration::append(Variable::PtrT &&var)
    {
        if (var == nullptr)
            throw NullException(this, ErrorLevel::CRITICAL, "appended null variable");
        var->set_parent(this);
        _variables.push_back(std::move(var));
        return true;
    }
    bool Declaration::prepend(Variable::PtrT const &var)
    {
        if (var == nullptr)
            throw NullException(this, ErrorLevel::CRITICAL, "appended null variable");
        var->set_parent(this);
        _variables.push_front(var);
        return true;
    }
    bool Declaration::prepend(Variable::PtrT &&var)
    {
        if (var == nullptr)
            throw NullException(this, ErrorLevel::CRITICAL, "appended null variable");
        var->set_parent(this);
        std::clog << std::format("prepend {}\n", var->name());
        _variables.push_front(std::move(var));
        return true;
    }

    Variable::UnownedPtrT Declaration::get_variable(std::string_view name)
    {
        for (auto &i : _variables) {
            if (i->name() == name)
                return i.get();
        }
        return nullptr;
    }
/* end class Declaration */

/* @class VarDecl */
    VarDecl::VarDecl(SourceRange const &range,
                     Type *base_type,
                     Node *parent, Scope *owner_scope)
        : Declaration(range, base_type, false, parent, owner_scope){ _node_type = NodeType::VAR_DECL; }
    
    bool VarDecl::accept(CodeVisitor *visitor) {
        _M_FAIL_IF_SCOPE_NULL(ErrorLevel::WARNING,
            std::format("unregistered variable declaration {{{}}}",
                             range().get_content()
            )
        );
        return visitor->visit(this);
    }
    bool VarDecl::accept_children(CodeVisitor *visitor)
    {
        for (auto &i: _variables) {
            if (i->accept(visitor) == false)
                return false;
        }
        return true;
    }
    bool VarDecl::register_this(Scope *owner)
    {
        if (owner == nullptr)
            return false;
        set_scope(owner);
        for (auto i: _variables) {
            i->set_base_type(this->get_base_type());
            if (i->register_this(owner) == false)
                return false;
        }
        return true;
    }
/* end class VarDecl */

/* @class ConstDecl */
    ConstDecl::ConstDecl(SourceRange const &range,
                     Type *base_type,
                     Node *parent, Scope *owner_scope)
        : Declaration(range, base_type, false, parent, owner_scope){
        _node_type = NodeType::CONST_DECL;
    }
    
    bool ConstDecl::accept(CodeVisitor *visitor) {
        _M_FAIL_IF_SCOPE_NULL(ErrorLevel::WARNING,
            std::format("unregistered variable declaration {{{}}}",
                             range().get_content()
            )
        );
        return visitor->visit(this);
    }
    bool ConstDecl::accept_children(CodeVisitor *visitor)
    {
        for (auto &i: _variables) {
            if (i->accept(visitor) == false)
                return false;
        }
        return true;
    }
    bool ConstDecl::register_this(Scope *owner)
    {
        if (owner == nullptr)
            return false;
        set_scope(owner);
        for (auto i: _variables) {
            i->set_base_type(_base_type);
            if (i->register_this(owner) == false)
                return false;
            std::clog << std::format("registered `{}`", i->name()) << std::endl;
        }
        return true;
    }
/* end class ConstDecl */
} // namespace MYGL::Ast
