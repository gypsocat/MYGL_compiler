#ifndef __MYGL_IRBASE_VALUE_VISITOR_H__
#define __MYGL_IRBASE_VALUE_VISITOR_H__

#include "base/mtb-object.hxx"
#include "irbase-use-def.hxx"
#include "ir-basic-value.hxx"
#include "ir-constant.hxx"
#include "ir-instruction.hxx"

namespace MYGL::IRBase {
using namespace IR;

interface IValueVisitor {
public:
    abstract void visit(IntConst   *value) {}
    abstract void visit(FloatConst *value) {}
    abstract void visit(ZeroConst  *value) {}
    abstract void visit(UndefinedConst *value) {}
    abstract void visit(PoisonConst *value) {}
    abstract void visit(ArrayExpr   *value) {}
    abstract void visit(Function    *value) {}
    abstract void visit(GlobalVariable *value) {}
    abstract void visit(BasicBlock *value) {}
    abstract void visit(Argument   *value) {}
    /* instructions */
    abstract void visit(PhiSSA    *value) {}
    abstract void visit(LoadSSA   *value) {}
    abstract void visit(CastSSA   *value) {}
    abstract void visit(UnaryOperationSSA *value) {}
    abstract void visit(MoveInst  *value) {}
    abstract void visit(AllocaSSA *value) {}
    abstract void visit(BinarySSA *value) {}
    abstract void visit(UnreachableSSA    *value) {}
    abstract void visit(JumpSSA   *value) {}
    abstract void visit(BranchSSA *value) {}
    abstract void visit(SwitchSSA *value) {}
    abstract void visit(BinarySelectSSA   *value) {}
    abstract void visit(CallSSA   *value) {}
    abstract void visit(ReturnSSA *value) {}
    abstract void visit(GetElemPtrSSA  *value) {}
    abstract void visit(ExtractElemSSA *value) {}
    abstract void visit(InsertElemSSA  *value) {}
    abstract void visit(StoreSSA   *value) {}
    abstract void visit(MemMoveSSA *value) {}
    abstract void visit(MemSetSSA  *value) {}
    abstract void visit(CompareSSA *value) {}
    abstract void visit(Module    *module) {}
protected:
    IValueVisitor() = default;
    virtual ~IValueVisitor() = default;
}; // interface IValueVisitor

}; // namespace MYGL::IR

#endif