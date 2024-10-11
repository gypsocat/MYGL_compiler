#include "../ir-instruction-base.hxx"
#include "../ir-basicblock.hxx"
#include <iostream>

namespace MYGL::Check {
    using namespace IR;
    using namespace MTB;

    namespace Instruction {
        inline void abort_if_inst_broken(IR::Instruction *inst)
        {
            // null 表示已析构, 可以放行
            if (inst == nullptr)
                return;
            // 有父结点表示是 unplug-plug 型，放行
            if (inst->get_parent() != nullptr)
                return;
            // 检查是否作为操作数使用, 没有的放行
            if (inst->get_list_as_usee().empty())
                return;
            // 接下来就是危险的孤儿结点了，终止程序
            std::clog << "Encountered Orphan Instruction "
                      << inst->get_name_or_id()
                      << " : Please check whether you unplugged an instruction"
                      << " while not re-plugged in another basic block"
                      << std::endl;
            abort();
        }

        template<typename Arg0, typename...Args>
        void abort_if_inst_broken(Arg0 inst0, Args...args)
        {
            abort_if_inst_broken(inst0);
            abort_if_inst_broken(args...);
        }
    }
}
