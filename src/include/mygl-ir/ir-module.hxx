#ifndef __MYGL_IR_MODULE_H__
#define __MYGL_IR_MODULE_H__

#include "base/mtb-getter-setter.hxx"
#include "base/mtb-object.hxx"
#include "ir-constant.hxx"
#include "ir-constant-function.hxx"
#include "irbase-type-context.hxx"
#include <string>
#include <string_view>
#include <unordered_map>

namespace MYGL::IR {
using namespace IRBase;
using namespace MTB;

/** @class Module
 * @brief MYGL-IR模块, 用于存放符号表、全局信息、IR结点树. */
class Module: public MTB::Object {
public:
    using GlobalVariableMapT = std::unordered_map<std::string, owned<GlobalVariable>>;
    using FunctionMapT       = std::unordered_map<std::string, owned<Function>>;
    using RefT               = MTB::owned<Module>;

    friend class     Value;
    friend interface IValueVisitor;

    static RefT Create(std::string_view name, size_t machine_word_size);
public:
    explicit Module(std::string_view name, size_t machine_word_size);
    ~Module() override;

    /** @property name{get;set;access;} string */
    DEFINE_GETSET(std::string_view, name)
    DEFINE_ACCESSOR(std::string, name)

    /** @property global_variables{get;access;} */
    DEFINE_TYPED_GETTER(GlobalVariableMapT const&, global_variables)
    DEFINE_ACCESSOR(GlobalVariableMapT, global_variables)

    /** @property functions{get;access;} */
    DEFINE_TYPED_GETTER(FunctionMapT const&, functions)
    DEFINE_ACCESSOR(FunctionMapT, functions)

    /** @property type_ctx{get;access;} */
    TypeContext const &get_type_ctx() const { return _type_ctx; }
    TypeContext &      type_ctx()           { return _type_ctx; }

    enum SetDefError: uint8_t {
        OK        = 0,
        ARG_NULL, ARG_TYPE_ERR,
        FN_EXIST, GVAR_EXIST,
    }; // enum SetFunctionError
    Function *getFunction(std::string_view name);
    Function *getFunction(std::string const& name);

    [[nodiscard("请及时处理可能返回的错误")]]
    SetDefError setFunction(std::string const& name, Function::RefT fn);
    
    [[nodiscard("请及时处理可能返回的错误")]]
    SetDefError setOrReplaceFunction(std::string const& name, Function::RefT fn);

    GlobalVariable *getGlobalVariable(std::string const& name);
    SetDefError     setGlobalVariable(std::string const& name, owned<GlobalVariable> gvar);
    SetDefError setOrReplaceGlobalVariable(std::string const& name, owned<GlobalVariable> gvar);

    Definition *getDefinition(std::string const& name);
    SetDefError setDefinition(std::string const& name, owned<Definition> definition);
    SetDefError setOrReplaceDefinition(std::string const& name, owned<Definition> definition);
private:
    std::string        _name;
    GlobalVariableMapT _global_variables;
    FunctionMapT       _functions;
    TypeContext        _type_ctx;
}; // class Module

} // namespace MYGL::IR

#endif