#pragma once
#include "base.hxx"
#include "ast-node-decl.hxx"
#include <memory>
#include <string_view>
#include <unordered_map>

namespace MYGL::Ast {

    /** @interface ScopeContainer */
    interface ScopeContainer {
        Node *owner_instance;
        abstract Scope *scope_self() = 0;
    }; // interface ScopeContainer

    /** @class Scope
     * @brief 作用域，存储变量定义、函数定义(SysY没有函数声明，以后可能会支持)
     * 
     * @var container{get;set;} 这个作用域所属的结点
     * 
     * @fn has_function(std::string_view) const -> bool
     * 检查当前作用域或其父作用域中是否存在指定名称的函数。
     *
     * @fn has_function_here(std::string_view) const -> bool
     * 仅检查当前作用域中是否存在指定名称的函数。
     *
     * @fn get_function(std::string_view) -> Function*
     * 获取当前作用域或其父作用域中指定名称的函数。
     *
     * @fn has_variable(std::string_view) const -> bool
     * 检查当前作用域或其父作用域中是否存在指定名称的变量。
     *
     * @fn get_variable(std::string_view) -> Variable*
     * 获取当前作用域或其父作用域中指定名称的变量。
     *
     * @fn has_constant(std::string_view) const -> bool
     * 检查当前作用域或其父作用域中是否存在指定名称的常量。
     *
     * @fn has_constant_here(std::string_view) const -> bool
     * 仅检查当前作用域中是否存在指定名称的常量。
     *
     * @fn get_constant(std::string_view) -> Variable*
     * 获取当前作用域或其父作用域中指定名称的常量。
     *
     * @fn has_constant_or_variable(std::string_view) const -> bool
     * 检查当前作用域或其父作用域中是否存在指定名称的常量或变量。
     *
     * @fn has_constant_or_variable_here(std::string_view) const -> bool
     * 仅检查当前作用域中是否存在指定名称的常量或变量。
     *
     * @fn get_constant_or_variable(std::string_view) -> Variable*
     * 获取当前作用域或其父作用域中指定名称的常量或变量。
     *
     * @fn has_definition(std::string_view) const -> bool
     * 检查当前作用域中是否存在指定名称的函数、常量或变量定义。
     *
     * @fn get_definition(std::string_view) -> Definition*
     * 获取当前作用域中指定名称的函数、常量或变量定义。
     *
     * @fn add(FuncPtrT) -> bool
     * 向当前作用域中添加一个函数定义。
     *
     * @fn add(VarPtrT) -> bool
     * 向当前作用域中添加一个变量定义。
     *
     * @fn remove(std::string_view) -> bool
     * 从当前作用域中移除指定名称的函数、常量或变量定义。
     *
     * @fn get_type(std::string_view) const -> Type*
     * 获取当前作用域中指定名称的类型定义。 */
    class Scope {
    public:
        using PtrT = std::unique_ptr<Scope>;
        using UnownedPtrT = Scope*;

        using VarPtrT  = Variable*;
        using FuncPtrT = Function*;
        using VarMapT  = std::unordered_map<std::string_view, VarPtrT>;
        using FuncMapT = std::unordered_map<std::string_view, FuncPtrT>;

        friend interface ScopeContainer;
    public:
        Scope(Scope *parent, ScopeContainer *container)
            : _parent(parent), _container(container) {}
        
        Scope *parent() const { return _parent; }
        ScopeContainer *container() const { return _container; }
        Node *owner_node() const { return _container->owner_instance; }
        VarMapT &variables() { return _variables; }
        VarMapT &constants() { return _constants; }
        FuncMapT &functions() { return _functions; }

        bool has_function(std::string_view name) const;
        bool has_function_here(std::string_view name) const {
            return _functions.contains(name);
        }
        FuncPtrT get_function(std::string_view name);
        bool has_variable(std::string_view name) const;
        bool has_variable_here(std::string_view name) const {
            return _variables.contains(name);
        }
        VarPtrT get_variable(std::string_view name);
        bool has_constant(std::string_view name) const;
        bool has_constant_here(std::string_view name) const {
            return _constants.contains(name);
        }
        VarPtrT get_constant(std::string_view name);

        bool has_constant_or_variable(std::string_view name) const;
        bool has_constant_or_variable_here(std::string_view name) const {
            return has_constant_here(name) || has_variable_here(name);
        }

        VarPtrT get_constant_or_variable(std::string_view name);
        inline bool has_definition(std::string_view name) const {
            return has_function(name) || has_constant(name) || has_variable(name);
        }
        Definition *get_definition(std::string_view name);

        bool add(FuncPtrT func);
        bool add(VarPtrT  var);
        bool remove(std::string_view name);

        Type *get_type(std::string_view name) const;
    private:
        Scope   *_parent;
        ScopeContainer *_container;
        VarMapT  _variables, _constants;
        FuncMapT _functions;
    }; // class Scope

} // namespace MYGL::Ast
