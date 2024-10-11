#ifndef __MYGL_IR_UTIL_FUNCTION_H__
#define __MYGL_IR_UTIL_FUNCTION_H__

#include "ir-constant-function.hxx"

namespace MYGL::IRUtil::Function {

/** @struct DirectCopyResult
 * @brief 函数拷贝结果, 包含函数新拷贝的指针和错误代码.
 *        函数指针不持有所有权. */
struct DirectCopyResult {
    enum Error: uint8_t {
        OTHERS = 0xFF,
        OK     = 0,
        NAME_EMPTY,   // 传入的名称为空
        NAME_EXISTED, // 相同名称的符号已存在
    }; // enum Error
public:
    IR::Function* copy;  // 函数的新拷贝
    enum Error    error; // 返回的错误
}; // struct DirectCopyResult

/** @fn direct_copy(func, name)
 * @brief 把函数 func 拷贝为一个新的函数, 新函数被重命名为 name.
 *        就算遇到直接递归函数调用，也不会修改被调用者的内容. */
[[nodiscard("请不要忽略拷贝得到的新函数, 以及可能存在的错误")]]
DirectCopyResult direct_copy(IR::Function* func, std::string_view new_name);

/** @fn gc_mark_sweep(function)
 * @brief 从入口基本块开始扫描函数的 CFG 和数据流关系, 遇到不可达
 *        且没有数据流关系的基本块时, 就删除这些基本块. */
void gc_mark_sweep(IR::Function *function);

} // namespace MYGL::IRUtil::Function

#endif