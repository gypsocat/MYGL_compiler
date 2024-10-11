#include "base/mtb-base.hxx"
#include "base/mtb-exception.hxx"
#include "mygl-ir/ir-basicblock.hxx"
#include "mygl-ir/ir-instruction-base.hxx"
#include "mygl-ir/ir-instruction.hxx"
#include "mygl-ir/irbase-use-def.hxx"
#include "mygl-ir/utils/irutil-basicblock.hxx"

namespace mygl_basicblock_impl {
    using namespace MYGL::IR;
    using TargetInfo = BasicBlock::TargetInfo;
    using RefT       = BasicBlock::RefT;

    delegate PhiReplacer {
        BasicBlock *old_src, *new_src;
    public:
        /** @return 和所有遍历函数一样, 返回 false 表示继续遍历,
         *          返回 true 表示终止遍历 */
        bool operator()(BasicBlock *target)
        {
            if (old_src == new_src)
                return true;
            if (target == new_src)
                return false;

            for (Instruction *i: target->get_instruction_list()) {
                if (i->get_type_id() != ValueTID::PHI_SSA)
                    continue;
                auto phi = static_cast<PhiSSA*>(i);
                Value *operand = phi->getValueFrom(old_src);
                phi->remove(old_src);
                phi->setValueFrom(new_src, operand);
            }
            return false;
        }
    }; // delegate BlockPhiReplacer

} // namespace mygl_basicblock_impl

namespace MYGL::IRUtil::BasicBlock {

namespace impl = ::mygl_basicblock_impl;
using namespace IR;
using namespace mygl_basicblock_impl;

IR::BasicBlock *split_end(IR::BasicBlock *current)
{
    if (current == nullptr) {
        crash_with_stacktrace(true, CURRENT_SRCLOC_F,
            "MYGL::IRUtil::BasicBlock::split_end(...)::current is [null]");
    }
    RefT ns = current->split(current->get_terminator()->get_instance());
    auto terminator = ns->get_terminator();
    terminator->traverse_targets(impl::PhiReplacer{current, ns.get()});
    return ns.get();
}

} // namespace MYGL::IRUtil::BasicBlock