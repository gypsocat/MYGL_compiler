#include "myglc-lang/ast-node-decl.hxx"
#include "myglc-lang/ast-node.hxx"
#include "myglc-lang/ast-code-visitor.hxx"
#include "myglc-lang/ast-scope.hxx"
#include "myglc-lang/ast-exception.hxx"
#include "myglc-lang/util.hxx"
#include <format>
#include <iostream>
#include <memory>
#include <string>


namespace MYGL::Ast {
/* @class ArrayInfo */
    bool ArrayInfo::accept(CodeVisitor *visitor) {
        return visitor->visit(this);
    }
    bool ArrayInfo::register_this(Scope *owner) {
        set_scope(owner);
        // std::cerr << "registered ArrayInfo" << std::endl;
        for (auto &i: _array_info) {
            if (i->register_this(owner) == false) {
                std::cerr << "ArrayInfo has error in registering its children" << std::endl;
                return false;
            }
        }
        return true;
    }

    bool ArrayInfo::append(Expression::PtrT const &next)
    {
        if (next == nullptr)
            return false;
        next->set_parent(this);
        next->set_scope(this->_scope);
        _array_info.push_back(next);
        return true;
    }
    bool ArrayInfo::append(Expression::PtrT &&next)
    {
        if (next == nullptr)
            return false;
        next->set_parent(this);
        next->set_scope(this->_scope);
        _array_info.push_back(std::move(next));
        return true;
    }
    bool ArrayInfo::prepend(Expression::PtrT const &next)
    {
        if (next == nullptr)
            return false;
        next->set_parent(this);
        next->set_scope(this->_scope);
        _array_info.push_front(next);
        return true;
    }
    bool ArrayInfo::prepend(Expression::PtrT &&next)
    {
        if (next == nullptr)
            return false;
        next->set_parent(this);
        next->set_scope(this->_scope);
        _array_info.push_front(std::move(next));
        return true;
    }
    std::string ArrayInfo::to_string() const
    {
        std::string ret;
        for (auto &i: _array_info) {
            auto conv = dynamic_cast<IntValue*>(i.get());
            if (conv != nullptr && conv->get_value() == 0)
                ret.append("[]");
            else
                ret.append(std::format("[{}]", i->range().get_content()));
        }
        return ret;
    }

    bool ArrayInfo::equals(ArrayInfo *rhs)
    {
        if (this == rhs)
            return true;
        return _array_info == rhs->_array_info;
    }
/* end class ArrayInfo */
/* @class Type */
    Type::PtrT Type::OwnedIntType = std::make_shared<Type>(SourceRange{}, nullptr, "int", &CompUnit::Predefined, CompUnit::Predefined.scope_self());
    Type::PtrT Type::OwnedFloatType = std::make_shared<Type>(SourceRange{}, &Type::VoidType, "float", &CompUnit::Predefined, CompUnit::Predefined.scope_self());
    Type::PtrT Type::OwnedVoidType = std::make_shared<Type>(SourceRange{}, nullptr, "void", &CompUnit::Predefined, CompUnit::Predefined.scope_self());
    Type &Type::IntType   = *OwnedIntType;
    Type &Type::FloatType = *OwnedFloatType;
    Type &Type::VoidType  = *OwnedVoidType;

    Type::Type(Type const &another)
        : Type(another._range,
                another.base_type(),
                another.type_name(),
                another.parent(),
                another.get_scope()) {
        _array_info = std::make_shared<ArrayInfo>(*another._array_info);
    }
    Type::Type(Type &&rranother)
        : Type(rranother._range,
                rranother.base_type(),
                rranother.type_name(),
                rranother.parent(),
                rranother.get_scope()) {
        _array_info = std::move(rranother._array_info);
    }
    bool Type::accept(CodeVisitor *visitor) {
        return visitor->visit(this);
    }
    std::string Type::to_string()
    {
        std::string ret {_name};
        if (_array_info != nullptr)
            ret.append(_array_info->to_string());
        return ret;
    }

    bool Type::equals(UnownedPtrT another)
    {
        if (this == another)
            return true;
        ArrayInfo *larray_info = _array_info.get();
        ArrayInfo *rarray_info = another->_array_info.get();
        if (larray_info == rarray_info)
            return true;
        if (larray_info == nullptr ||
            rarray_info == nullptr)
            return false;
        if (larray_info->equals(rarray_info) == false)
            return false;
        return (_base_type == another->_base_type) &&
               (_name      == another->_name);
    }
/* end class Type */

/* @class Variable */
    bool Variable::accept(CodeVisitor *visitor) {
        if (_scope == nullptr) {
            throw NullException(this, ErrorLevel::WARNING,
                std::format("Unregistered {} `{}`", is_constant()? "constant":"variable", name()));
        }
        return visitor->visit(this);
    }
    bool Variable::accept_children(CodeVisitor *visitor)
    {
        bool ret = true;
        if (_array_info != nullptr)
            ret &= _array_info->accept(visitor);
        if (_init_expr != nullptr)
            ret &= _init_expr->accept(visitor);
        return ret;
    }
    bool Variable::register_this(Scope *owner_scope)
    {
        set_scope(owner_scope);
        if (owner_scope == nullptr)
            return false;
        if (_array_info != nullptr) {
            if (_array_info->register_this(owner_scope) == false)
                return false;
        }
        if (_init_expr != nullptr)
            _init_expr->register_this(owner_scope);
        return owner_scope->add(this);
    }

    void Variable::set_init_expr(Expression::PtrT const &value)
    {
        if (value != nullptr)
            value->set_parent(this);
        _init_expr = value;
    }
    void Variable::set_init_expr(Expression::PtrT &&value)
    {
        if (value != nullptr)
            value->set_parent(this);
        _init_expr = std::move(value);
    }

    Type::PtrT Variable::get_real_type() const
    {
        if (_array_info == nullptr || _array_info->array_size() == 0)
            return std::static_pointer_cast<Type>(shared_from_this());
        Type::PtrT ret = std::make_shared<Type>(*_base_type);
        if (ret->get_array_info() == nullptr) {
            ret->set_array_info(std::make_shared<ArrayInfo>(_array_info));
        } else {
            ArrayInfo::PtrT info     = std::move(ret->get_array_info());
            ArrayInfo::PtrT new_info = std::make_shared<ArrayInfo>(_array_info);
            ArrayInfo::InfoListT &new_info_list = new_info->array_info();
            for (auto &i: info->array_info())
                new_info_list.push_back(std::move(i));
            ret->set_array_info(std::move(new_info));
        }
        return ret;
    }

    bool Variable::is_func_param() const {
        if (_parent_node == nullptr)
            return false;
        return _parent_node->node_type() == NodeType::FUNC_PARAM;
    }
/* end class Variable */

/* @class FuncParam */
    FuncParam::FuncParam(SourceRange const &range)
        : Node(range, NodeType::FUNC_PARAM) {}
    
    bool FuncParam::accept(CodeVisitor *visitor)
    {
        if (_scope == nullptr || _parent_node == nullptr) {
            throw NullException(this, ErrorLevel::WARNING, 
                "Unregistered FuncParam");
        }
        return visitor->visit(this);
    }
    bool FuncParam::accept_children(CodeVisitor *visitor)
    {
        for (auto &i: _param_list) {
            if (i->accept(visitor) == false)
                return false;
        }
        return true;
    }
    bool FuncParam::register_this(Scope *owner)
    {
        set_scope(owner);
        for (auto &i: _param_list) {
            if (i->register_this(owner) == false)
                return false;
        }
        return true;
    }

    Function *FuncParam::get_function() const {
        auto ret = dynamic_cast<Function*>(_parent_node);
        return ret;
    }
    void FuncParam::set_function(Function *parent) {
        _parent_node = parent;
    }
    bool FuncParam::append(Variable::PtrT &&param)
    {
        if (param == nullptr)
            return false;
        param->set_parent(this);
        _param_list.push_back(std::move(param));
        return true;
    }
    bool FuncParam::prepend(Variable::PtrT &&param)
    {
        if (param == nullptr)
            return false;
        param->set_parent(this);
        _param_list.push_front(std::move(param));
        return true;
    }
/* end class FuncParam */

/* @class Function */
    Function::Function(SourceRange const &range,
                       Type *return_type, std::string_view func_name,
                       FuncParam::PtrT &&func_param,
                       Block::PtrT &&fn_body)
        : Definition(range, return_type, func_name, nullptr, nullptr),
          _func_params(std::move(func_param)),
          _func_body(std::move(fn_body)),
          _scope_self(nullptr) {
        _node_type = NodeType::FUNC_DEF;
        _func_params->set_parent(this);
        _func_body->set_parent(this);
        owner_instance = this;
    }

    bool Function::accept(CodeVisitor *visitor)
    {
        if (_scope_self == nullptr || _scope == nullptr) {
            throw NullException(this, ErrorLevel::CRITICAL,
                std::format("Unregistered function `{}`", name()));
        }
        return visitor->visit(this);
    }
    bool Function::accept_children(CodeVisitor *visitor) {
        return (_func_params->accept(visitor)) && (_func_body->accept(visitor));
    }
    bool Function::register_this(Scope *owner)
    {
        if (owner == nullptr)
            return false;
        _scope_self = std::make_unique<Scope>(owner, this);
        set_scope(owner);
        return _func_params->register_this(_scope_self.get()) &&
               _func_body->register_this(_scope_self.get());
    }
    void Function::set_func_params(FuncParam::PtrT &&fn_params)
    {
        if (fn_params != nullptr)
            fn_params->set_parent(this);
        _func_params = std::move(fn_params);
    }
/* end class Function */
} // namespace MYGL::Ast
