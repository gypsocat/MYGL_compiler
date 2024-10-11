#pragma once
#include "base/mtb-compatibility.hxx"
#include "base/mtb-exception.hxx"
#include "base/mtb-object.hxx"
#include "../ir-instruction-base.hxx"
#include "../irbase-value-visitor.hxx"
#include "mygl-ir/ir-instruction.hxx"
#include "mygl-ir/irbase-use-def.hxx"
#include <cstddef>
#include <map>
#include <set>
#include <unordered_set>

namespace MYGL::IRUtil {

using namespace IR;

/** @class SideEffectDetective
 * @brief 检测函数中的指令是否可能存在副作用. 在MYGL中，
 *        有副作用的指令不允许删除.
 * @warning 由于一个函数结点内部随时会发生变化，该检测器不会缓存检测的结果. */
class SideEffectDetective: public MTB::Object,
                           public IRBase::IValueVisitor {
public:
    using BlockSetT  = std::unordered_set<BasicBlock*>;
    using UserSetT   = std::set<Instruction*>;
    using TargetSetT = std::set<Value*>;
    using TargetMapT = std::map<Instruction*, TargetSetT>;
public:
    /** @fn detect_once
     * @brief 检测一条指令，并返回指令是否具有副作用.
     *        当检测到指令具有副作用时就停下来.
     *
     * 该检测器会扫描指令所有的前驱和后继. 假定前驱或
     * 后继有副作用,则指令就有副作用. */
    bool   detect_once(Instruction *inst, BlockSetT const* exclusive = nullptr);

    /** @fn detect_all
     * @brief 检测一条指令, 并返回指令具有的副作用列表. */
    size_t detect_all (Instruction *inst,
                       TargetSetT  &out_side_effects,
                       BlockSetT const* exclusive = nullptr);
    
    /** @fn detect_cached
     * @brief   检测一条指令, 同时更新指令的副作用缓存.
     * @warning 由于该类不存储状态, 该方法不保证函数结点发生改变时,
     *          副作用缓存还会起作用. 因此该函数仅仅适用于指令节点
     *          不可变的情况. */
    void detect_cached(Instruction *inst,
                       TargetSetT  &out_side_effects,
                       TargetMapT  &ref_side_effect_cache,
                       BlockSetT const* exclusive = nullptr);

private:
    void visit(PhiSSA     *value) override;
    void visit(LoadSSA    *value) override;
    void visit(CastSSA    *value) override;
    void visit(UnaryOperationSSA *value) override;
    void visit(AllocaSSA  *value) override;
    void visit(BinarySSA  *value) override;
    void visit(JumpSSA    *value) override;
    void visit(BranchSSA  *value) override;
    void visit(SwitchSSA  *value) override;
    void visit(CallSSA    *value) override;
    void visit(ReturnSSA  *value) override;
    void visit(GetElemPtrSSA *value) override;
    void visit(StoreSSA   *value) override;
    void visit(CompareSSA *value) override;

    /* 该指令非 SSA 指令, 遇到 MoveInst 时需要立即终止程序. */
    void visit(MoveInst   *value) override {
        Compatibility::crash(true, CURRENT_SRCLOC_F,
            Compatibility::osb_fmt("Detected MoveInst ", value, " in stage 1 SSA process"));
    }
public:
    Instruction *inst;
}; // class SideEffectDetective

} // namespace MYGL::IRUtil 
