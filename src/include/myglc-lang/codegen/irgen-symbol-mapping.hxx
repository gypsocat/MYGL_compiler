#pragma once

#include "base/mtb-object.hxx"
#include "mygl-ir/ir-basic-value.hxx"
#include "mygl-ir/ir-basicblock.hxx"
#include "mygl-ir/ir-constant-function.hxx"
#include "mygl-ir/ir-constant.hxx"
#include "mygl-ir/ir-instruction-base.hxx"
#include "mygl-ir/ir-module.hxx"
#include "mygl-ir/irbase-use-def.hxx"
#include "myglc-lang/ast-node-decl.hxx"
#include <cstdint>
#include <map>

namespace MYGL::IRGen::SymbolMapping {

    enum class SetDefError: uint8_t {
        OK, DEF_EXISTED
    }; // enum class SetDefError

    struct Info {
        enum Kind: uint8_t {
            NONE = 0x0,
            INSTRUCTION,
            CONST_DATA,
            GLOBAL_VAR,
            FUNC_PARAM,
        }; // enum Kind
        Ast::Variable            *definition;
        MTB::owned<IRBase::Value> storage = nullptr;
        enum Kind                 kind    = NONE;
    public:
        IR::Instruction *get_instruction_value() const;
        void set_instruction_value(IR::Instruction *value);

        IR::ConstantData *get_constdata_value() const;
        void set_constdata_value(IR::ConstantData *value);

        IR::Argument *get_param_value() const;
        void set_param_value(IR::Argument *value);

        IR::GlobalVariable *get_gvar_value() const;
        void set_gvar_value(IR::GlobalVariable *value);
    public:
        static Info CreateNone() {
            return {nullptr, {nullptr}, NONE};
        }
        bool is_none() const {
            return kind == NONE || storage == nullptr ||
                   definition == nullptr;
        }
    }; // struct Info

    /** @class FunctionLocalMap
     * @brief 函数作用域内的符号映射表.
     * 该映射表存放 3 种关系：函数内常量, 函数内变量定义, 参数定义 */
    class FunctionLocalMap: public MTB::Object {
    public:
        /** @brief 根据语法树的变量定义获取结点树值定义的工具 */
        using DefMapT = std::map<Ast::Variable*, Info>;

    public:
        /** 初始化 AST 函数和 IR 函数, 并建立参数之间的映射关系 */
        FunctionLocalMap(Ast::Function *afunc, IR::Function *ifunc);
        /** 在源文件中实现，防止 vtable 出现问题用的 */
        ~FunctionLocalMap() override;

        bool hasDefinition(Ast::Variable *vardef) const {
            return getDefinition(vardef).is_none();
        }
        Info getDefinition(Ast::Variable *vardef) const;
        [[nodiscard("请务必处理返回的 SetDefError 错误.")]]
        SetDefError setDefinition(Info info);

        /** @property ast_func{get;} */
        Ast::Function *get_ast_func() const { return _ast_func; }

        /** @property ir_func{get;} */
        IR::Function  *get_ir_func()  const { return _ir_func; }
    private:
        Ast::Function *_ast_func;
        IR::Function   *_ir_func;
        DefMapT         _def_map;
    }; // class FunctionLocalMap

    class GlobalMap: public MTB::Object {
    public:
        using VarDefMapT   = std::map<Ast::Variable*, IR::GlobalVariable*>;
        using FunctionMapT = std::map<Ast::Function*, FunctionLocalMap>;
    public:
        FunctionMapT &func_map() { return _func_map; }
        VarDefMapT   &gvar_def() { return _gvar_def; }

        FunctionLocalMap &getRegisterFunction(Ast::Function *afunc);

        IR::GlobalVariable *getGlobalVariable(Ast::Variable *key);
        SetDefError        *setGlobalVariable(Ast::Variable *key, IR::GlobalVariable *value);

        IR::Module *get_module() const;
    private:
        VarDefMapT      _gvar_def;
        FunctionMapT    _func_map;
        IR::Module     *_module;
    }; // class GlobalMap

}

namespace MYGL::IRGen::SymbolMapping {

    inline IR::Instruction* Info::get_instruction_value() const
    {
        if (kind != INSTRUCTION)
            return nullptr;
        return static_cast<IR::Instruction*>(storage.get());
    }
    inline void Info::set_instruction_value(IR::Instruction *value)
    {
        this->storage = value;
        this->kind    = INSTRUCTION;
    }
    
    inline IR::ConstantData *Info::get_constdata_value() const
    {
        if (kind != CONST_DATA)
            return nullptr;
        return static_cast<IR::ConstantData*>(storage.get());
    }
    inline void Info::set_constdata_value(IR::ConstantData *value)
    {
        this->storage = value;
        this->kind    = CONST_DATA;
    }

    inline IR::Argument *Info::get_param_value() const
    {
        if (kind != CONST_DATA)
            return nullptr;
        return static_cast<IR::Argument*>(storage.get());
    }
    inline void Info::set_param_value(IR::Argument *value)
    {
        this->storage = value;
        this->kind    = FUNC_PARAM;
    }

    inline IR::GlobalVariable* Info::get_gvar_value() const
    {
        if (kind != CONST_DATA)
            return nullptr;
        return static_cast<IR::GlobalVariable*>(storage.get());
    }
    inline void Info::set_gvar_value(IR::GlobalVariable *value)
    {
        this->storage = value;
        this->kind    = GLOBAL_VAR;
    }
}