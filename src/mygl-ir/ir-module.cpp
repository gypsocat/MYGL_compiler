#include "mygl-ir/ir-module.hxx"
#include "mygl-ir/ir-constant.hxx"
#include "mygl-ir/ir-constant-function.hxx"
#include <string>
#include <string_view>

namespace MYGL::IR {

inline namespace module_impl {
    static Module::RefT new_mod(std::string_view name, size_t machine_word_size) {
        RETURN_MTB_OWN(Module, name, machine_word_size);
    }
} // inline namespace module_impl

/** ================ [static class Module] ================ */
Module::RefT Module::Create(std::string_view name, size_t machine_word_size) {
    return new_mod(name, machine_word_size);
}

/** ================ [public class Module] ================ */
Module::Module(std::string_view name, size_t machine_word_size)
    : _name(name),
      _type_ctx(machine_word_size) {
}
Module::~Module() = default;

Function *Module::getFunction(std::string const& name)
{
    auto iter = _functions.find(name);
    if (iter == _functions.end())
        return nullptr;
    return iter->second.get();
}
Function *Module::getFunction(std::string_view name) {
    return getFunction(std::string{name});
}
Module::SetDefError Module::
setFunction(std::string const& name, Function::RefT fn)
{
    if (fn == nullptr)
        return SetDefError::ARG_NULL;

    auto fiter = _functions.find(name);
    if (fiter != _functions.end())
        return FN_EXIST;

    auto viter = _global_variables.find(name);
    if (viter != _global_variables.end())
        return GVAR_EXIST;

    _functions.insert({name, std::move(fn)});
    return SetDefError::OK;
}
Module::SetDefError Module::
setOrReplaceFunction(std::string const& name, owned<Function> fn)
{
    if (fn == nullptr)
        return SetDefError::ARG_NULL;

    auto viter = _global_variables.find(name);
    if (viter != _global_variables.end())
        return GVAR_EXIST;

    _functions.insert_or_assign(name, std::move(fn));
    return SetDefError::OK;
}

GlobalVariable *Module::getGlobalVariable(std::string const& name)
{
    auto iter = _global_variables.find(name);
    if (iter == _global_variables.end())
        return nullptr;
    return iter->second.get();
}
Module::SetDefError Module::
setGlobalVariable(std::string const& name, owned<GlobalVariable> gvar)
{
    if (gvar == nullptr)
        return SetDefError::ARG_NULL;

    auto fiter = _functions.find(name);
    if (fiter != _functions.end())
        return FN_EXIST;

    auto viter = _global_variables.find(name);
    if (viter != _global_variables.end())
        return GVAR_EXIST;

    _global_variables.insert({name, std::move(gvar)});
    return SetDefError::OK;
}
Module::SetDefError Module::
setOrReplaceGlobalVariable(std::string const& name, owned<GlobalVariable> gvar)
{
    if (gvar == nullptr)
        return SetDefError::ARG_NULL;

    auto fiter = _functions.find(name);
    if (fiter != _functions.end())
        return FN_EXIST;

    _global_variables.insert_or_assign(name, std::move(gvar));
    return SetDefError::OK;
}

Definition *Module::
getDefinition(std::string const& name)
{
    Definition *ret;
    if ((ret = getFunction(name)) != nullptr)
        return ret;
    if ((ret = getGlobalVariable(name)) != nullptr)
        return ret;
    return nullptr;
}
Module::SetDefError Module::
setDefinition(std::string const& name, owned<Definition> definition)
{
    if (definition == nullptr)
        return SetDefError::ARG_NULL;
    if (definition->is_function())
        return setFunction(name, definition.static_get<Function>());
    if (definition->is_global_variable())
        return setGlobalVariable(name, definition.static_get<GlobalVariable>());
    return SetDefError::ARG_TYPE_ERR;
}
Module::SetDefError Module::
setOrReplaceDefinition(std::string const& name, owned<Definition> definition)
{
    if (definition == nullptr)
        return SetDefError::ARG_NULL;

    if (auto fiter = _functions.find(name);
        fiter != _functions.end()) {
        if (definition.get() == fiter->second.get())
            return SetDefError::OK;
        if (definition->is_function())
            fiter->second = definition.static_get<Function>();
        _functions.erase(fiter);
    }

    if (auto viter = _global_variables.find(name);
        viter != _global_variables.end()) {
        if (definition.get() == viter->second.get())
            return SetDefError::OK;
        if (definition->is_global_variable())
            viter->second = definition.static_get<GlobalVariable>();
        _global_variables.erase(viter);
    }

    if (definition->is_function()) {
        _functions.insert({name, definition.static_get<Function>()});
        return SetDefError::OK;
    }
    if (definition->is_global_variable()) {
        _global_variables.insert({
            name, definition.static_get<GlobalVariable>()
        });
        return SetDefError::OK;
    }

    return SetDefError::ARG_TYPE_ERR;
}

} // namespace MYGL::IR
