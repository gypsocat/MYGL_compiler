#pragma once

#if __cplusplus < 202002L
#include <cstdint>
#include <atomic>
#include <type_traits>
#include <utility>

#ifndef __MTB_OBJECT_H__
#error You should only include mtb-object.hxx instead of using this header.
#endif

#define PublicExtendsObject typename
#define ASSERT_PUBLIC_EXT_OBJECT static_assert(std::is_base_of<Object, ObjT>::value, "ObjT type should publicly extend Object");
namespace MTB {

using pointer = void*;
// C++对bool类型没有规定, 这里定1字节
struct bool8_t {
    inline bool8_t() noexcept: value(false){}
    inline bool8_t(bool value) noexcept: value(value){}
    inline bool8_t(bool8_t const& value) noexcept: value(value.value){}
    inline bool8_t(bool8_t &&value) noexcept: value(value.value){}
    inline bool8_t(uint8_t const u8) noexcept: value(u8 != 0){}
    template<typename IntegerT>
    inline bool8_t(IntegerT const i): value(i != 0){}

    inline operator bool() const noexcept { return value; }
    inline operator uint8_t() const noexcept { return value; }
    template<typename IntegerT>
    operator IntegerT() const { return IntegerT(value); }
    bool operator==(bool const &rhs) const noexcept { return value == rhs; }
    bool operator!=(bool const &rhs) const noexcept { return value != rhs;  }
    bool operator==(bool8_t const rhs) const noexcept { return value == rhs.value; }
    bool operator!=(bool8_t const rhs) const noexcept { return value != rhs.value; }
    template<typename IntegerT>
    bool operator==(IntegerT const rhs) const noexcept { return (value != 0) == (rhs != 0); }
    template<typename IntegerT>
    bool operator!=(IntegerT const rhs) const noexcept { return (value != 0) != (rhs != 0); }

    bool8_t &operator=(bool8_t const &value) = default;
    bool8_t &operator=(bool8_t &&value) = default;
    bool8_t &operator=(bool value) {
        this->value = value;
        return *this;
    }
private:
    alignas(uint8_t) uint8_t value;
};


namespace FallBack {

/** @class Object
    * @brief Medi ToolBox的基础类, 有基于引用计数自动内存管理功能. 不使用`std::shared_ptr`
    *        的理由是, `shared_ptr`很容易出现重复析构的问题.
    * @brief 如果你喜欢的话, 可以让你的所有类都继承`Object`. 这样我写运行时泛型就方便了. */
class Object {
public:
    Object() : __ref_count__(0) {}
    virtual ~Object() = default;
    alignas(uint64_t) mutable volatile std::atomic_int __ref_count__;

    inline void setInlineInit()   const { __ref_count__++; }
    inline void unsetInlineInit() const { __ref_count__--; }
}; // class Object

template<typename ObjET>
struct InlineInit {
    template<typename...Args>
    InlineInit(Args...args)
        : instance(args...) {
        instance.setInlineInit();
    }

    operator ObjET &() & { return instance; }
    operator ObjET const&() const& { return instance; }
    operator ObjET &&() && { return std::move(instance); }
    operator ObjET *() & { return &instance; }
    ObjET instance;
}; // class InlineInit<ObjET> requires {Object: Object}

template<typename ObjT>
struct owned {
    ObjT *__ptr;
    owned(): __ptr(nullptr) {ASSERT_PUBLIC_EXT_OBJECT}
    owned(std::nullptr_t): __ptr(nullptr) {ASSERT_PUBLIC_EXT_OBJECT}

    owned(ObjT        *obj): owned()     { ASSERT_PUBLIC_EXT_OBJECT ref0(obj); }
    owned(owned const& obj): owned()     { ASSERT_PUBLIC_EXT_OBJECT ref0(obj.__ptr); }
    owned(owned &&obj): __ptr(obj.__ptr) { ASSERT_PUBLIC_EXT_OBJECT obj.__ptr = nullptr; }

    template<typename ObjET, typename = typename std::enable_if<std::is_base_of_v<ObjT, ObjET>, std::true_type>::type>
    owned(ObjET *obj): __ptr(static_cast<ObjT>(obj)) { ASSERT_PUBLIC_EXT_OBJECT ref(); }

    ObjT *operator->() { return __ptr; }
    ObjT const* operator->() const { return __ptr; }
    ObjT &operator*() { return *__ptr; }
    ObjT const& operator*() const { return *__ptr; }
    operator ObjT*() const& { return __ptr; }
    operator ObjT*() && = delete;
    template<typename ObjPT, typename = typename std::enable_if<std::is_base_of_v<ObjPT, ObjT>, std::true_type>::type>
    operator owned<ObjPT>() { return owned<ObjPT>(__ptr); }

    owned &operator=(const ObjT *rhs) {
        unref0(); ref(rhs);
    }
    owned &operator=(owned const& rhs) {
        unref0();
        ref0(rhs.__ptr);
        return *this;
    }
    owned &operator=(owned &&rhs) {
        std::swap(rhs.__ptr, __ptr);
        return *this;
    }
    template<typename ObjET, typename = typename std::enable_if<std::is_base_of_v<ObjT, ObjET>, std::true_type>::type>
    owned &operator=(owned<ObjET> const &rhs) {
        unref0();
        ref0(rhs.__ptr);
        return *this;
    }
    template<typename ObjET, typename = typename std::enable_if<std::is_base_of_v<ObjT, ObjET>, std::true_type>::type>
    owned &operator=(owned<ObjET> &&rhs) {
        unref0();
        __ptr = rhs.__ptr;
        rhs.__ptr = nullptr;
        return *this;
    }
    template<typename ObjET>
    owned &operator=(owned<ObjET> const &rhs) {
        if (rhs.__ptr == nullptr) {
            unref0(); return *this;
        }
        ObjT *rhsp = dynamic_cast<ObjT*>(rhs.__ptr);
        if (rhsp == nullptr) return *this;

        unref0(); ref0(rhsp);
        return *this;
    }


public:
    void ref() const { const_cast<ObjT*>(__ptr)->__ref_count__++; }
    void unref() {
        __ptr->__ref_count__--;
        if (__ptr->__ref_count__ == nullptr)
            delete __ptr;
    }
    void reset() { unref0(); }
    ObjT *get() const { return __ptr; }
    ObjT *steal_get() {
        ObjT *ret = __ptr;
        ret->__ref_count__--;
        __ptr = nullptr;
        return ret;
    }
    template<typename ObjET>
    ObjET *static_get() const { return static_cast<ObjET*>(__ptr); }
    template<typename ObjET>
    ObjET *dynamic_get() const { return static_cast<ObjET*>(__ptr); }
protected:
    void ref0(const ObjT* ptr) {
        if (ptr == nullptr) return;
        __ptr = const_cast<ObjT*>(ptr);
        ref();
    }
    void unref0() { if (__ptr) unref(); __ptr = nullptr; }
}; // struct owned

template<typename ObjT, typename = typename std::enable_if<std::is_assignable_v<Object*, ObjT*>>>
using unowned = ObjT*;

} // namespace MTB::FallBack

using namespace FallBack;

} // namespace MTB

#endif