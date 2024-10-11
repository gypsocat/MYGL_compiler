#ifndef __MTB_CLASSED_ENUM_H__
#define __MTB_CLASSED_ENUM_H__

/** @file mtb-classed-enum.hxx
 * @brief Medi's ToolBox结构化枚举宏.
 *
 * 有一些枚举会自带大量的方法, 这时把枚举转化为一个类更合适.
 * 最好的方案应该是使用模板, 但是知道书写方法以后你会知道, 使用
 * 模板是不可行的，所以我定义了一些宏来辅助书写。
 *
 * 创建一个结构化枚举Foo的方法如下:
 * - 把`enum Foo`替换为`struct Foo`.
 * - 在`Foo`结构体内新建一个枚举类型`self_t`, 把enum的所有内容
 *   放进`self_t`中
 * - 在`Foo`结构体的末尾加上`MTB_MAKE_CLASSED_ENUM(Foo)`
 *
 * 这样你就得到了一个结构化枚举. 在这个结构化枚举中, 你可以:
 * - 与无符号整数任意转换
 * - 执行与、或、异或运算, 把enum变成二进制掩码
 * - 在enum里放方法 */
#define MTB_MAKE_CLASSED_ENUM(EnumType) \
public:\
    constexpr EnumType() noexcept: self(self_t(0)){}\
    constexpr EnumType(EnumType const &that) noexcept: self(that.self) {}\
    constexpr EnumType(EnumType &&that) noexcept: self(that.self){}\
    constexpr inline EnumType(self_t self) noexcept: self(self){}\
    constexpr inline EnumType(size_t self) noexcept: self(self_t(self)){}\
    \
    operator self_t() const { return self; }\
    constexpr bool operator==(self_t rhs) const { return self == rhs; }\
    constexpr bool operator!=(self_t rhs) const { return self != rhs; }\
    \
    EnumType &operator=(EnumType const &that) = default;\
    EnumType &operator=(EnumType &&that) = default;\
    \
    EnumType operator|(EnumType that) const {\
        return EnumType(size_t(self) | size_t(that.self));\
    }\
    EnumType operator|(self_t that) const {\
        return EnumType(size_t(self) | size_t(that));\
    }\
    EnumType operator|(size_t that) const {\
        return EnumType(size_t(self) | that);\
    }\
    EnumType &operator|=(EnumType that) {\
        self = self_t(size_t(self) | size_t(that.self));\
        return *this;\
    }\
    EnumType &operator|=(self_t that) {\
        self = self_t(size_t(self) | size_t(that));\
        return *this;\
    }\
    EnumType &operator|=(size_t that) {\
        self = self_t(size_t(self) | that);\
        return *this;\
    }\
    EnumType operator&(EnumType that) const {\
        return EnumType(size_t(self) & size_t(that.self));\
    }\
    EnumType operator&(self_t that) const {\
        return EnumType(size_t(self) & size_t(that));\
    }\
    EnumType operator&(size_t that) const {\
        return EnumType(size_t(self) & that);\
    }\
    EnumType &operator&=(EnumType that) {\
        self = self_t(size_t(self) & size_t(that.self));\
        return *this;\
    }\
    EnumType &operator&=(self_t that) {\
        self = self_t(size_t(self) & size_t(that));\
        return *this;\
    }\
    EnumType &operator&=(size_t that) {\
        self = self_t(size_t(self) & that);\
        return *this;\
    }\
    EnumType operator^(EnumType that) const {\
        return EnumType(size_t(self) ^ size_t(that.self));\
    }\
    EnumType operator^(self_t that) const {\
        return EnumType(size_t(self) ^ size_t(that));\
    }\
    EnumType operator^(size_t that) const {\
        return EnumType(size_t(self) ^ that);\
    }\
    EnumType &operator^=(EnumType that) {\
        self = self_t(size_t(self) ^ size_t(that.self));\
        return *this;\
    }\
    EnumType &operator^=(self_t that) {\
        self = self_t(size_t(self) ^ size_t(that));\
        return *this;\
    }\
    EnumType &operator^=(size_t that) {\
        self = self_t(size_t(self) ^ that);\
        return *this;\
    }\
protected:\
    self_t self;\
public:

#endif