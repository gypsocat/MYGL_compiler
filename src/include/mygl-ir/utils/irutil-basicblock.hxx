#pragma once
#include "mygl-ir/ir-basicblock.hxx"

namespace MYGL::IRUtil::BasicBlock {

    /** @fn split_end(current) -> splitted
     * @brief 从基本块的末尾拆分基本块, 并保留 PhiSSA 等数据流关系 */
    IR::BasicBlock *split_end(IR::BasicBlock *current);

    enum class RecomposeError: uint8_t {
        OK = 0,
        TARGET_NONE,     // 没有跳转目标, 多见于非 JumpSSA 结尾的
        TARGET_TOO_MANY, // 跳转目标有不止一个
    }; // enum class RecomposeError
    static inline char const* recompose_err_get_reason(RecomposeError err) noexcept
    {
        char const* reasons[] = {
            "OK",
            "current basic block has no target",
            "current basic block has more than 1 targets",
        };
        return reasons[unsigned(err)];
    }
    /** @fn recompose(current)
     * @brief 倘若 current 只有一个跳转目标, 则把 current 和跳转
     *        目标合并为一个基本块 */
    [[nodiscard("请不要忽略可能返回的 RecomposeError 错误.")]]
    RecomposeError recompose(IR::BasicBlock *current);

} // namespace MYGL::IRUtil::BasicBlock