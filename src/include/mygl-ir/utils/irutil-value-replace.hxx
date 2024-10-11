#pragma once
#ifndef __MYGL_IRUTIL_VALUE_REPLACE_H__
#define __MYGL_IRUTIL_VALUE_REPLACE_H__
#include "mygl-ir/irbase-use-def.hxx"

namespace MYGL::IRUtil {

    /** @fn usee_replace_this_with(old_usee -> new_usee)
     * @brief 扫描所有以值 `old_usee` 为操作数的 `User`, 然后把该 `User`
     *        的所有 `old_usee` 替换为 `new_usee`.
     * @return owned<Value> 清除以后的 `old_usee`. 该值不会被立即析构.
     * @throws NullException 当 `old_usee` 或者 `new_usee` 二者有一个
     *         为 `null` 时抛出该异常.
     * @throws MTB::Exception 由具体的检查函数抛出 */
    extern MTB::owned<IRBase::Value>
    usee_replace_this_with(IRBase::Value *old_usee, IRBase::Value *new_usee);

} // namespace MYGL::IRUtil

#endif