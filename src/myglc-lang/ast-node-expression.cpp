/** @file ast-node-expression.cpp
 * @brief 语句结点的实现 */

#include <exception>
#include <format>
#include <iostream>
#include <memory>
#include <ostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include "myglc-lang/ast-node-decl.hxx"
#include "myglc-lang/ast-node.hxx"
#include "myglc-lang/ast-code-visitor.hxx"
#include "myglc-lang/ast-scope.hxx"
#include "myglc-lang/util.hxx"
#include "myglc-lang/ast-exception.hxx"

#define _M_SET_EXPRESSION_EX(child, expr, true_return, false_return) {\
    if ((expr) != nullptr) {\
        (expr)->set_parent(this);\
        child = (expr);\
        true_return;\
    }\
    false_return;\
}
#define _M_SET_EXPRESSION(child, expr) \
    _M_SET_EXPRESSION_EX(child, expr, {}, {})
#define _M_SET_EXPRESSION_BOOL(child, expr) \
    _M_SET_EXPRESSION_EX(child, expr, {return true;}, {return false;})

#define _M_MOVE_EXPRESSION_EX(child, rrexpr, true_return, false_return) {\
    if ((rrexpr) != nullptr) {\
        (rrexpr)->set_parent(this);\
        child = std::move(rrexpr);\
        {true_return};\
    }\
    {false_return};\
}
#define _M_MOVE_EXPRESSION(child, rrexpr) \
    _M_MOVE_EXPRESSION_EX(child, rrexpr, {}, {})
#define _M_MOVE_EXPRESSION_BOOL(child, rrexpr) \
    _M_MOVE_EXPRESSION_EX(child, rrexpr, {return true;}, {return false;})

#define _M_LIST_ADD_EXPR_EX(child, expr, op, true_return, false_return) {\
    if ((expr) == nullptr) { false_return; }\
    (expr)->set_parent(this);\
    child.op(expr);\
    {true_return};\
}
#define _M_LIST_ADD_EXPR(child, expr, op) \
    _M_LIST_ADD_EXPR_EX(child, expr, op, {}, {})
#define _M_LIST_ADD_EXPR_BOOL(child, expr, op) \
    _M_LIST_ADD_EXPR_EX(child, expr, op, {return true;}, {return false;})

#define _M_LIST_MOVE_EXPR_EX(child, expr, op, true_return, false_return) {\
    if ((expr) == nullptr) { false_return; }\
    (expr)->set_parent(this);\
    child.op(std::move(expr));\
    {true_return};\
}
#define _M_LIST_MOVE_EXPR(child, expr, op) \
    _M_LIST_MOVE_EXPR_EX(child, expr, op, {}, {})
#define _M_LIST_MOVE_EXPR_BOOL(child, expr, op) \
    _M_LIST_MOVE_EXPR_EX(child, expr, op, {return true;}, {return false;})

namespace MYGL::Ast {
/* @enum Expression::Operator */
    Expression::Operator Expression::OperatorFromStr(std::string_view op) {
        static std::unordered_map<std::string_view, Operator> op_map = {
            {"+", Operator::PLUS},
            {"-", Operator::SUB},
            {"*", Operator::STAR},
            {"/", Operator::SLASH},
            {"%", Operator::PERCENT},
            {"&&", Operator::AND},
            {"||", Operator::OR},
            {"!",  Operator::NOT},
            {"=", Operator::ASSIGN},
            {"+=", Operator::PLUS_ASSIGN},
            {"-=", Operator::SUB_ASSIGN},
            {"*=", Operator::MUL_ASSIGN},
            {"/=", Operator::DIV_ASSIGN},
            {"%=", Operator::MOD_ASSIGN},
        };
        if (!op_map.contains(op))
            return Operator::NONE;
        return op_map[op];
    }
/* end enum Expression::Operator */
/* @class Expression */
    Expression::Expression(SourceRange const &range,
                           Node *parent, Scope *owner_scope,
                           NodeType type, bool is_lvalue)
        : Node(range, type, parent, owner_scope),
          _is_lvalue(is_lvalue){}
/* end class Expression */

/* @class UnaryExpr */
    UnaryExpr::UnaryExpr(SourceRange const &range,
                         Operator xoperator, Expression::PtrT &&expr,
                         Node *parent)
        : Expression(range, parent),
          _xoperator(xoperator),
          _expression(std::move(expr)) {
        _node_type = NodeType::UNARY_EXPR;
        _expression->set_parent(this);
    }

    /* extends Node */
    bool UnaryExpr::accept(CodeVisitor *visitor) {
        if (_scope == nullptr)
            throw NullException(this, ErrorLevel::WARNING, "uninitialized scope");
        return visitor->visit(this);
    }
    bool UnaryExpr::accept_children(CodeVisitor *visitor) {
        if (_scope == nullptr)
            throw NullException(this, ErrorLevel::WARNING, "uninitialized scope");
        return _expression->accept(visitor);
    }
    bool UnaryExpr::register_this(Scope *owner_scope) {
        _scope = owner_scope;
        _expression->set_scope(owner_scope);
        return _expression->register_this(owner_scope);
    }

    /* extends Expression */
    std::shared_ptr<UnaryExpr::VarListT> UnaryExpr::get_variable_list() {
        if (_expression == nullptr)
            return std::make_shared<UnaryExpr::VarListT>();
        return _expression->get_variable_list();
    }

    void UnaryExpr::set_expression(Expression::PtrT const &expr)
        _M_SET_EXPRESSION(_expression, expr)
    void UnaryExpr::set_expression(Expression::PtrT &&expr)
        _M_MOVE_EXPRESSION(_expression, expr)
/* end class UnaryExpr */

/* @class BinaryExpr */
    BinaryExpr::BinaryExpr(SourceRange const &range, Operator xoperator,
                           Expression::PtrT &&lhs, Expression::PtrT &&rhs,
                           Node *parent)
        : Expression(range, parent),
          _xoperator(xoperator),
          _lhs(std::move(lhs)), _rhs(std::move(rhs)){
        _node_type = NodeType::BINARY_EXPR;
        _lhs->set_parent(this);
        _rhs->set_parent(this);
    }

    bool BinaryExpr::accept(CodeVisitor *visitor)
    {
        if (_scope == nullptr)
            throw NullException(this, ErrorLevel::WARNING, "uninitialized scope");
        return visitor->visit(this);
    }
    bool BinaryExpr::accept_children(CodeVisitor *visitor)
    {
        if (_scope == nullptr)
            return false;
        return _lhs->accept(visitor) && _rhs->accept(visitor);
    }
    bool BinaryExpr::register_this(Scope *owner_scope)
    {
        _scope = owner_scope;
        return _lhs->register_this(owner_scope) &&
               _rhs->register_this(owner_scope);
    }
    std::shared_ptr<Expression::VarListT> BinaryExpr::get_variable_list()
    {
        if (_scope == nullptr)
            return nullptr;
        auto lhs_list = _lhs->get_variable_list();
        auto rhs_list = _rhs->get_variable_list();
        for (auto &i: *rhs_list) {
            /* 注意这里可能会出现的内存问题. */
            if (!lhs_list->contains(i.first))
                lhs_list->insert(std::move(i));
        }
        return lhs_list;
    }

    void BinaryExpr::set_lhs(Expression::PtrT const &lhs)
        _M_SET_EXPRESSION(_lhs, lhs)
    void BinaryExpr::set_lhs(Expression::PtrT &&rlhs)
        _M_MOVE_EXPRESSION(_lhs, rlhs)
    void BinaryExpr::set_rhs(Expression::PtrT const &rhs)
        _M_SET_EXPRESSION(_rhs, rhs)
    void BinaryExpr::set_rhs(Expression::PtrT &&rrhs)
        _M_MOVE_EXPRESSION(_rhs, rrhs)

/* end class BinaryExpr */

/* @class CallParam */
    CallParam::CallParam(SourceRange const &range)
        : Expression(range){
        _node_type = NodeType::CALL_PARAM;
    }

    bool CallParam::accept(CodeVisitor *visitor) {
        if (_scope == nullptr)
            register_this(visitor->current_scope());
        return visitor->visit(this);
    }
    bool CallParam::accept_children(CodeVisitor *visitor) {
        if (_scope == nullptr)
            register_this(visitor->current_scope());
        for (Expression::PtrT &i: _expression) {
            if (i->accept(visitor) == false)
                return false;
        }
        return true;
    }
    bool CallParam::register_this(Scope::UnownedPtrT owner_scope)
    {
        _scope = owner_scope;
        for (Expression::PtrT &i: _expression) {
            if (i->register_this(owner_scope) == false)
                return false;
        }
        return true;
    }
    std::shared_ptr<Expression::VarListT> CallParam::get_variable_list()
    {
        auto ret = std::make_shared<Expression::VarListT>();
        if (_scope == nullptr)
            return ret;
        for (auto &i: _expression) {
            auto child_list = i->get_variable_list();
            for (auto &j: *child_list) {
                if (!ret->contains(j.first))
                    ret->insert(j);
            }
        }
        return ret;
    }

    bool CallParam::append(Expression::PtrT const &expr)
        _M_LIST_ADD_EXPR_BOOL(_expression, expr, push_back)
    bool CallParam::append(Expression::PtrT &&expr)
        _M_LIST_MOVE_EXPR_BOOL(_expression, expr, push_back)
    bool CallParam::prepend(Expression::PtrT const &expr)
        _M_LIST_ADD_EXPR_BOOL(_expression, expr, push_front)
    bool CallParam::prepend(Expression::PtrT &&expr)
        _M_LIST_MOVE_EXPR_BOOL(_expression, expr, push_front)
/* end class CallParam */

/* @class CallExpr */
    CallExpr::CallExpr(SourceRange const &range,
                       Identifier::PtrT &&name, CallParam::PtrT  &&param)
        : Expression(range),
          _name(name), _param(param) {
        _node_type = NodeType::CALL_EXPR;
        _name->set_parent(this);
        _param->set_parent(this);
    }

    bool CallExpr::accept(CodeVisitor *v) {
        return _scope != nullptr && v->visit(this);
    }
    bool CallExpr::accept_children(CodeVisitor *v) {
        return _name->accept(v) && _param->accept(v);
    }
    bool CallExpr::register_this(Scope::UnownedPtrT os)
    {
        set_scope(os);
        /* register function name*/
        _name->set_scope(os);
        std::string_view name = _name->name();
        if (!os->has_function(name)) {
            std::clog << std::format(
                "Detected external function `{}`",
                name
            ) << std::endl;
        } else {
            _name->set_definition(os->get_function(name));
        }
        return _param->register_this(os);
    }
    std::shared_ptr<Expression::VarListT> CallExpr::get_variable_list() {
        return _param->get_variable_list();
    }

    void CallExpr::set_name(Identifier::PtrT const &name)
        _M_SET_EXPRESSION(_name, name)
    void CallExpr::set_name(Identifier::PtrT &&name)
        _M_MOVE_EXPRESSION(_name, name)
    void CallExpr::set_param(CallParam::PtrT const &param)
        _M_SET_EXPRESSION(_param, param)
    void CallExpr::set_param(CallParam::PtrT &&param)
        _M_MOVE_EXPRESSION(_param, param)
/* end class CallExpr */

/* @class InitList */
    InitList::InitList(SourceRange const &range)
        : Expression(range) {
        _node_type = NodeType::INIT_LIST;
    }

    bool InitList::accept(CodeVisitor *v) {
        return _scope != nullptr && v->visit(this);
    }
    bool InitList::accept_children(CodeVisitor *v) {
        if (_scope == nullptr)
            return false;
        for (auto &i: _expr_list) {
            if (i->accept(v) == false)
                return false;
        }
        return true;
    }
    bool InitList::register_this(Scope::UnownedPtrT os)
    {
        set_scope(os);
        for (auto &i: _expr_list) {
            if (i->register_this(os) == false)
                return false;
        }
        return true;
    }

    bool InitList::append(Expression::PtrT const &item)
        _M_LIST_ADD_EXPR_BOOL(_expr_list, item, push_back)
    bool InitList::append(Expression::PtrT &&item)
        _M_LIST_MOVE_EXPR_BOOL(_expr_list, item, push_back)
    bool InitList::prepend(Expression::PtrT const &item)
        _M_LIST_ADD_EXPR_BOOL(_expr_list, item, push_front)
    bool InitList::prepend(Expression::PtrT &&item)
        _M_LIST_MOVE_EXPR_BOOL(_expr_list, item, push_front)
    
    std::shared_ptr<Expression::VarListT> InitList::get_variable_list() {
        return nullptr;
    }
/* end class InitList*/
/* @class IndexExpr */
    IndexExpr::IndexExpr(SourceRange const &range, Identifier::PtrT &&name)
        : Expression(range), _name(name) {
        _node_type = NodeType::INDEX_EXPR;
    }

    /* extends Node */
    bool IndexExpr::accept(CodeVisitor *visitor) {
        return _scope != nullptr && visitor->visit(this);
    }
    bool IndexExpr::accept_children(CodeVisitor *visitor)
    {
        if (_scope == nullptr)
            return false;
        for (auto i: _index_list) {
            if (i->accept(visitor) == false)
                return false;
        }
        return true;
    }
    bool IndexExpr::register_this(Scope::UnownedPtrT scope)
    {
        set_scope(scope);
        if (_name->register_this(scope) == false)
            return false;
        for (auto &i: _index_list) {
            if (i->register_this(scope) == false)
                return false;
        }
        return true;
    }

    bool IndexExpr::append(Expression::PtrT const &index)
        _M_LIST_ADD_EXPR_BOOL(_index_list, index, push_back)
    bool IndexExpr::append(Expression::PtrT &&index)
        _M_LIST_MOVE_EXPR_BOOL(_index_list, index, push_back)
    bool IndexExpr::prepend(Expression::PtrT const &index)
        _M_LIST_ADD_EXPR_BOOL(_index_list, index, push_front)
    bool IndexExpr::prepend(Expression::PtrT &&index)
        _M_LIST_MOVE_EXPR_BOOL(_index_list, index, push_front)
    
    std::shared_ptr<Expression::VarListT> IndexExpr::get_variable_list() {
        auto ret = _name->get_variable_list();
        for (auto &i: _index_list) {
            auto cur = i->get_variable_list();
            for (auto &j: *cur) {
                if (!ret->contains(j.first))
                    ret->insert(j);
            }
        }
        return ret;
    }
/* end class IndexExpr */

/* @class Identifier */
    Identifier::Identifier(SourceRange const &range)
        : Expression(range) {
        _node_type = NodeType::IDENT;
        _is_lvalue = true;
    }

    bool Identifier::accept(CodeVisitor *visitor)
    {
        if (_scope == nullptr) {
            throw NullException(this, ErrorLevel::WARNING,
                                "Identifier Unregistered");
        }
        return visitor->visit(this);
    }
    bool Identifier::accept_children(CodeVisitor *visitor) {
        (void)visitor;
        return true;
    }
    bool Identifier::register_this(Scope::UnownedPtrT owner_scope) {
        set_scope(owner_scope);
        if (owner_scope == nullptr) {
            throw NullException(this, ErrorLevel::WARNING,
                std::format("(Identifier \"{}\") owner scope is NULL", name()));
        }
        if (!owner_scope->has_definition(name())) {
            std::clog << std::format("owner scope does not have definition of `{}`",
                name()) << std::endl;
            return false;
        }
        _definition = owner_scope->get_definition(name());
        Variable::UnownedPtrT def_as_var = dynamic_cast<Variable*>(_definition);
        _is_lvalue = (def_as_var != nullptr) && !(def_as_var->is_constant());
        return true;
    }

    std::shared_ptr<Expression::VarListT> Identifier::get_variable_list() {
        auto ret = std::make_shared<Expression::VarListT>();
        ret->insert({name(), dynamic_cast<Variable*>(_definition)});
        return ret;
    }
/* end class Identifier */

/* @class Value, StringValue, IntValue, FloatValue */
    Value::Value(SourceRange const &range, NodeType type)
        : Expression(range) {
        _node_type = type;
    }

    IntValue::IntValue(SourceRange const &range)
        : Value(range, NodeType::INT_VALUE) {
        std::string value_string(range.get_content());
        std::stringstream is(value_string);
        try {
            is >> _value;
        } catch(std::exception &e) {
            std::cout << e.what() << std::endl;
            _value = 0;
        }
    }

    FloatValue::FloatValue(SourceRange const &range)
        : Value(range, NodeType::FLOAT_VALUE) {
        std::string value_string(range.get_content());
        std::stringstream is(value_string);
        try {
            is >> _value;
        } catch (std::exception &e) {
            std::cout << e.what() << std::endl;
            _value = 0.0;
        }
    }

    bool StringValue::accept(CodeVisitor *visitor) {
        return visitor->visit(this);
    }
    bool IntValue::accept(CodeVisitor *visitor) {
        return visitor->visit(this);
    }
    bool FloatValue::accept(CodeVisitor *visitor) {
        return visitor->visit(this);
    }

    std::string IntValue::value_string() {
        return std::format("{}", _value);
    }
    std::string FloatValue::value_string() {
        return std::format("{:g}", _value);
    }
/* end class Value */
/* @class AssignExpr */
    AssignExpr::AssignExpr(SourceRange const &range,
                           Expression::PtrT &&source, Expression::PtrT &&destination)
        : Expression(range), _source(source), _destination(destination) {
        _node_type = NodeType::ASSIGN_EXPR;
    }

    bool AssignExpr::accept(CodeVisitor *visitor) {
        if (_scope == nullptr) {
            throw NullException(this, ErrorLevel::WARNING,
                                "Empty Scope");
        }
        return visitor->visit(this);
    }
    bool AssignExpr::accept_children(CodeVisitor *visitor) {
        return _destination->accept(visitor) && _source->accept(visitor);
    }
    bool AssignExpr::register_this(Scope *owner_scope)
    {
        if (owner_scope == nullptr)
            return false;
        set_scope(owner_scope);
        return (_source->register_this(owner_scope)) &&
               (_destination->register_this(owner_scope));
    }

    void AssignExpr::set_destination(Expression::PtrT const &destination)
        _M_SET_EXPRESSION(_destination, destination)
    void AssignExpr::set_destination(Expression::PtrT &&destination)
        _M_MOVE_EXPRESSION(_destination, destination)
    void AssignExpr::set_source(Expression::PtrT const &source)
        _M_SET_EXPRESSION(_source, source)
    void AssignExpr::set_source(Expression::PtrT &&source)
        _M_MOVE_EXPRESSION(_source, source)

    std::shared_ptr<Expression::VarListT> AssignExpr::get_variable_list() {
        return nullptr;
    }
/* end class AssignExpr */
} // namespace MYGL::Ast

#undef _M_SET_EXPRESSION
