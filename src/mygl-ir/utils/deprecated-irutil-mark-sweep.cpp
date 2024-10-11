#include "base/mtb-object.hxx"
#include "mygl-ir/ir-constant.hxx"
#include "mygl-ir/ir-instruction-base.hxx"
#include "mygl-ir/utils/irutil-sideeffect.hxx"
#include "../irbase-capatibility.private.hxx"

namespace MYGL::IRUtil {
    using namespace IR;
    using namespace capatibility;

    using BlockSetT = std::unordered_set<BasicBlock*>;
    using UserSetT  = std::unordered_set<Instruction*>;

    static void trace_orphan_instruction(BlockSetT const& unused_basic_block,
                                         Instruction *inst,
                                         UserSetT &orphan_inst);

    static void trace_lonely_operands(BlockSetT const& unused_basic_block,
                                      UserSetT   &out_orphan_instructions)
    {
        for (BasicBlock *unused : unused_basic_block) {
            for (Instruction *inst: unused->instruction_list()) {
                trace_orphan_instruction(unused_basic_block, inst, out_orphan_instructions);
            }
        }
    }

    static void trace_orphan_instruction(BlockSetT const& unused_basic_block,
                                         Instruction    * inst,
                                         UserSetT       & orphan_inst)
    {
        for (Use *use: inst->list_as_usee()) {
            User *user = use->get_user();
            Instruction *inst = dynamic_cast<Instruction*>(user); 
            if (inst == nullptr)
                continue;
            if (inst->get_list_as_usee().empty() &&
                IRUtil::SideEffectDetective{}.detect_once(inst))

            if (user->get_type_id() == ValueTID::PHI_SSA) {
                PhiSSA *user_as_phi = static_cast<PhiSSA*>(user);
                /* 假定该Phi函数只有自己一个元素, 那么该Phi函数就是自己了.
                 * 跟踪该Phi函数直到发现其他孤儿指令为止. */
                if (user_as_phi->get_operands().size() <= 1)
                    trace_orphan_instruction(unused_basic_block, user_as_phi, orphan_inst);
                
            }
        }
    }

    delegate BodyMarkSweep {
        using BlockIterT      = BasicBlock::ListT::Modifier;
        using DeadBlockCacheT = std::vector<BlockIterT>;
    public:
        Function             *function;
        Function::BlockListT     &body;
        BlockSetT            alive_set;

        BodyMarkSweep(Function *fn)
            :function(fn), body(fn->function_body()) {
        }

        size_t operator()()
        {
            mark(function->get_first_basicblock());
            sweep();
            return alive_set.size();
        }

        void mark(BasicBlock *block)
        {
            for (auto i: block->get_jumps_to()) {
                if (!contains(alive_set, i.target_block)) {
                    alive_set.insert(i.target_block);
                    mark(i.target_block);
                }
            }
        }
        void sweep()
        {
            DeadBlockCacheT iter_buff(body.size() - alive_set.size());
            size_t iter_buff_len = 0;
            for (BasicBlock *i: body) {
                if (!contains(alive_set, i)) {
                    iter_buff[iter_buff_len] = i->get_iterator();
                    iter_buff_len++;
                }
            }
            for (auto &i: iter_buff) {
                i.remove_this();
            }
        }
    }; // struct BodyMarkSweep
} // namespace MYGL::IRUtil

