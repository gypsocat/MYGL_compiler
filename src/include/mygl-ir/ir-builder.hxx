#ifndef __MYGL_IR_BUILDER_H__
#define __MYGL_IR_BUILDER_H__

#include "base/mtb-getter-setter.hxx"
#include "base/mtb-object.hxx"
#include "irbase-value-visitor.hxx"
#include "ir-module.hxx"
#include "ir-basic-value.hxx"
#include "ir-constant.hxx"
#include <string_view>

namespace MYGL::IR {
using namespace MYGL::IRBase;

class Builder: public MTB::Object,
               public IValueVisitor {
public:
    using string_view = std::string_view;
public:
    explicit Builder();

    DEFINE_TYPED_GETTER(Module*, module)

    [[nodiscard("Builder.steal_module()-> Module generated")]]
    owned<Module> steal_module() { return std::move(_module); }

    DEFINE_GETSET(Function*, current_function)

    DEFINE_GETSET(BasicBlock*, current_block)

    Module *createModule(string_view name);

    template<typename TypeT>
#if __cplusplus >= 202002L
    requires std::is_base_of_v<Type, TypeT>
#endif
    TypeT *addType(TypeT *ty) {
        if (_module == nullptr)
            return nullptr;
        Type *ret = _module->type_ctx().getOrRegisterType(ty);
        return static_cast<TypeT*>(ty);
    }
    Type *addType(owned<Type> ty);
    template<typename TyName, typename... Args>
#if __cplusplus >= 202002L
    requires std::is_base_of_v<Type, TyName>
#endif
    TyName *makeType(Args...args) {
        if (get_module() == nullptr)
            return nullptr;
        TypeContext &ctx = _module->type_ctx();
        return ctx.makeType<TyName>(args...);
    }

    Function *declareFunction(string_view name, FunctionType *func_type);
    Function *defineFunction(string_view name, FunctionType *func_type);
    Function *selectFunction(string_view name);
    Function *selectFunction(Function *fn);
    void removeFunction(string_view name);
    void removeFunction(Function   *fn) {
        removeFunction(fn->get_name());
    }

    GlobalVariable *declareGlobalVariable(string_view name, Type *data_type);
    GlobalVariable *defineGlobalVariable(string_view name, Type *data_type, owned<Constant> init_expr);
    GlobalVariable *getGlobalVariable(std::string_view name);
    void removeGlobalVariable(string_view     name);
    void removeGlobalVariable(GlobalVariable *gvar) {
        removeGlobalVariable(gvar->get_name());
    }
    GlobalVariable *declareGlobalConstant(string_view name, Type *data_type);
    GlobalVariable *defineGlobalConstant(string_view name, Type *data_type, owned<Constant> init_expr);

    void addDefine(owned<Function> fn);
    void addDefine(owned<GlobalVariable> gvar);

    BasicBlock *addBasicBlock() {
        return addBasicBlock(new BasicBlock(get_current_function()));
    }
    BasicBlock *addBasicBlock(owned<BasicBlock> block);
    BasicBlock *splitBasicBlock(Instruction *inst);
    BasicBlock *selectBasicBlock(BasicBlock *block);

    Instruction *addInstruction(owned<Instruction> inst);
    Instruction *replaceInstruction(Instruction *before, owned<Instruction> after);
public:
    void visit(IntConst   *value) override;
    void visit(FloatConst *value) override;
    void visit(ZeroConst  *value) override;
    void visit(UndefinedConst *value) override;
    void visit(Array      *value) override;
    void visit(Function   *value) override;
    void visit(GlobalVariable *value) override;
    void visit(BasicBlock *value) override;
    void visit(Argument   *value) override;

    void visit(PhiSSA    *value) override;
    void visit(LoadSSA   *value) override;
    void visit(CastSSA   *value) override;
    void visit(UnaryOperationSSA *value) override;
    void visit(AllocaSSA *value) override;
    void visit(BinarySSA *value) override;
    void visit(JumpSSA   *value) override;
    void visit(BranchSSA *value) override;
    void visit(SwitchSSA *value) override;
    void visit(CallSSA   *value) override;
    void visit(ReturnSSA *value) override;
    void visit(GetElemPtrSSA *value) override;
    void visit(StoreSSA   *value) override;
    void visit(MoveInst   *value) override;
    void visit(MemMoveSSA *value) override;
    void visit(MemSetSSA  *value) override;
    void visit(CompareSSA *value) override;
    void visit(Module    *module) override;
private:
    owned<Module> _module;
    Function     *_current_function;
    BasicBlock   *_current_block;
}; // class Builder

} // namespace MYGL::IR

#endif