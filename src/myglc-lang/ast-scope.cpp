#include "myglc-lang/ast-scope.hxx"
#include "myglc-lang/ast-node.hxx"
#include <string_view>


namespace MYGL::Ast {
    bool Scope::has_function(std::string_view name) const
    {
        return (_functions.contains(name) ||
                (_parent != nullptr &&
                 _parent->has_function(name)));
    }
    Function *Scope::get_function(std::string_view name)
    {
        if (_functions.contains(name))
            return _functions[name];
        else if (_parent != nullptr)
            return _parent->get_function(name);
        else
            return nullptr;
    }
    bool Scope::has_variable(std::string_view name) const
    {
        return (_variables.contains(name) ||
                (_parent != nullptr &&
                 _parent->has_variable(name)));
    }

    Variable *Scope::get_variable(std::string_view name)
    {
        if (_variables.contains(name))
            return _variables[name];
        else if (_parent != nullptr)
            return _parent->get_variable(name);
        else
            return nullptr;
    }

    bool Scope::has_constant(std::string_view name) const
    {
        return (_constants.contains(name) ||
                (_parent != nullptr &&
                 _parent->has_constant(name)));
    }

    Variable *Scope::get_constant(std::string_view name)
    {
        if (_constants.contains(name))
            return _constants[name];
        else if (_parent != nullptr)
            return _parent->get_constant(name);
        else
            return nullptr;
    }
    Variable *Scope::get_constant_or_variable(std::string_view name)
    {
        if (_constants.contains(name))
            return _constants[name];
        else if (_variables.contains(name))
            return _variables[name];
        else if (_parent != nullptr)
            return _parent->get_constant_or_variable(name);
        else
            return nullptr;
    }

    Definition *Scope::get_definition(std::string_view name)
    {
        if (_functions.contains(name))
            return _functions[name];
        else if (_constants.contains(name))
            return _constants[name];
        else if (_variables.contains(name))
            return _variables[name];
        else if (_parent != nullptr)
            return _parent->get_definition(name);
        else
            return nullptr;
    }

    bool Scope::add(Function::UnownedPtrT func)
    {
        if (has_function_here(func->name()))
            return false;
        func->set_scope(this);
        _functions.insert({func->name(), func});
        return true;
    }
    bool Scope::add(Variable::UnownedPtrT var)
    {
        if (has_variable_here(var->name()))
            return false;
        var->set_scope(this);
        VarMapT *target_map = var->is_constant() ? &_constants: &_variables;
        target_map->insert({var->name(), var});
        return true;
    }

    bool Scope::remove(std::string_view name)
    {
        if (has_constant_here(name))
            _constants.erase(name);
        else if (has_variable_here(name))
            _variables.erase(name);
        else if (has_function_here(name))
            _functions.erase(name);
        else
            return false;

        return true;
    }

} // namespace MYGL::Ast
