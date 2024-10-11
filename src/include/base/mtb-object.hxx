#ifndef __MTB_OBJECT_H__
#define __MTB_OBJECT_H__

#include "mtb-base.hxx"

#if __cplusplus >= 202002L

#include <atomic>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <utility>

namespace MTB {
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
        void __inc_refcnt() const { __ref_count__.fetch_add(1); }
        bool __dec_refcnt_and_test() const {
            int ret = --__ref_count__;
            return (ret == 0);
        }
    }; // class Object

#if __cplusplus > 202002L
    /** @class PublicExtendsObject concept
     * @brief 验证一个类是否继承自Object. */
    template<typename ObjT>
    concept PublicExtendsObject = requires(ObjT &x, Object *y) {
        y = &x;
    };
#else
#   define PublicExtendsObject typename
#endif

    template<PublicExtendsObject ObjET>
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
    }; // class InlineInit<ObjET> requires {Objet: Object}

    /** @struct owned
     * @brief   和`Object`搭配使用的智能指针, 可以自动管理引用计数。你可以像std::shared_ptr
     *          那样使用MTB::make_owned模板创建，也可以直接从普通指针赋值。由于引用计数就在对
     *          象本体里面，所以不用担心shared_ptr的多重释放问题。
     * @warning 和所有其他采用引用计数的智能指针一样, 滥用这种持有所有权的指针可能会导致循环引用问题!
     * @warning **你不能把owned右值引用隐式转换为普通指针.** 如果非要这么做，请使用get()或者steal_get()
     *          方法, 并注意会不会出现UAF问题。详细的解决方案在下文。
     * @fn operator=
     * @fn operator*
     * @fn operator<=>
     * @fn get() const 获取owned指向的地址, 且不改变owned的引用
     * @fn steal_get() 窃取owned指向的地址, owned会失去对目标的引用. 调用者有义务维护目标的
     *                 生命周期。
     *
     * - 类型转换交互: 你可以像普通指针一样使用owned智能指针，但是有一个例外——你不能把owned右值
     *   引用隐式转换为普通指针。这是为了防止这样的场景出现:
     *
     *   你定义了一个普通指针变量, 并使用MTB::make_owned表达式或者随便一个什么返回owned的函数
     *   初始化它。结果因为owned指针在一开始就释放了目标, 你的普通指针变成了一个野指针.
     *
     *   遇到这种情况请检查你的调用者调用了什么函数。如果是make_owned模板，请替换为new表达式. 如果
     *   是返回owned指针的函数，那就替换成返回普通指针的函数(MYGL允许以普通指针的形式返回持有所有权
     *   的指针)。如果是别人写的话就联系作者。
     *   上述情况都莫得的话，你需要显式使用get()方法或者steal_get()方法获取普通指针。前者获取到的
     *   指针没有所有权，后者有所有权。 */
    template<PublicExtendsObject ObjT>
    struct owned {
        ObjT *__ptr;

        /** 构造函数 -- 无参构造得null */
        owned() noexcept : __ptr(nullptr) {}
        owned(std::nullptr_t) noexcept: __ptr(nullptr){}
        /** 构造函数 -- 从实例指针构造 */
        owned(ObjT *instance) noexcept {
            __ptr = instance;
            instance->__ref_count__++;
        }
        /** 构造函数 -- 从同类型持有所有权的指针构造*/
        owned(owned const &instance) noexcept {
            __ptr = instance.__ptr;
            __ptr->__ref_count__++;
        }
        /** 构造函数 -- 移动构造 */
        owned(owned &&rrinst) noexcept {
            __ptr = rrinst.__ptr;
            rrinst.__ptr = nullptr;
        }
        /** 析构时解引用 */
        ~owned() {
            if (__ptr == nullptr)
                return;
            __ptr->__ref_count__--;
            if (__ptr->__ref_count__ == 0)
                delete __ptr;
        }

        /** 模板：从其他类型构造，会dynamic_cast */
        template<typename ObjET>
        MTB_REQUIRE ((std::is_base_of<ObjT, ObjET>::value &&
                     !std::is_same_v<ObjT, ObjET>))
        owned(ObjET *ext) {
            __ptr = ext;
            ext->__ref_count__++;
        }
        template<typename ObjET>
        MTB_REQUIRE ((std::is_base_of<ObjT, ObjET>::value &&
                     !std::is_same_v<ObjT, ObjET>))
        owned(const ObjET *ext) {
            __ptr = const_cast<ObjET*>(ext); ref();
        }
        template<typename ObjET>
        MTB_REQUIRE ((std::is_base_of<ObjT, ObjET>::value &&
                     !std::is_same_v<ObjT, ObjET>))
        owned(owned<ObjET> &ext) {
            auto ptr = static_cast<ObjT*>(ext.__ptr);
            __ptr = ptr;
            if (ptr != nullptr)
                ptr->__ref_count__++;
        }
        template<typename ObjET>
        MTB_REQUIRE ((std::is_base_of<ObjT, ObjET>::value &&
                     !std::is_same_v<ObjT, ObjET>))
        owned(owned<ObjET> const &ext) {
            auto ptr = static_cast<ObjT*>(ext.__ptr);
            __ptr = ptr;
            if (ptr != nullptr)
                ptr->__ref_count__++;
        }
        template<typename ObjET>
        MTB_REQUIRE ((std::is_base_of<ObjT, ObjET>::value))
        owned(owned<ObjET> &&ext) {
            __ptr = static_cast<ObjT*>(ext.__ptr);
            ext.__ptr = nullptr;
        }

        /** 与普通指针交互、取地址 */
        operator ObjT* () const& { return __ptr; }
        /** @fn operator ObjT*
         * 为了减少一个普通指针隐式地从owned初始化导致的UAF问题, 你不能把一个owned右值引用
         * 转换为普通指针。在owned结构体的注释中有解决方案. */
        operator ObjT* () && = delete;

        /** @fn operator ParentT* */
        template<typename ParentT>
        requires (std::is_base_of_v<ParentT, ObjT> &&
                 !std::is_base_of_v<ObjT, ParentT>)
        operator ParentT*() const& {
            return static_get<ParentT>();
        }
        template<typename ChildT>
        requires (std::is_base_of_v<ObjT, ChildT> &&
                 !std::is_base_of_v<ChildT, ObjT>)
        operator ChildT*() const& {
            return dynamic_get<ChildT*>();
        }
        /** @fn operator ObjET*(this &&) = delete
         * @brief 为了防止内存问题, 从右值转换到任意类型的普通指针都是禁止的.
         *
         * 若有必要，请使用`steal_get()`方法来转移所有权。转移得到所有权后记得释放。 */
        template<PublicExtendsObject ObjET>
        operator ObjET*() && = delete;

        /** @fn operator owned<T> */
        template<typename ObjET>
        requires (!(std::is_base_of_v<ObjET, ObjT>))
        operator owned<ObjET>() const {
            return owned(dynamic_cast<ObjET*>(__ptr));
        }

        ObjT *get() const { return __ptr; }
        /** @fn steal_get
         * @brief 从此owned实例中窃取指向对象的指针，并将所有权转移给调用者。调用此方法后，
         *        owned实例不再持有对该对象的引用。如果因此操作导致对象的引用计数归零，调
         *        用者有责任删除该对象。
         * @return 指向对象的指针。如果此实例未持有任何对象，则返回空指针。*/
        [[nodiscard("Object.steal_get() -> {owned instance pointer}")]]
        ObjT *steal_get()
        {
            if (__ptr == nullptr) return nullptr;
            __ptr->__ref_count__--;
            ObjT *ret = __ptr;
            __ptr = nullptr;
            return ret;
        }
        /** 使用内存重解释的版本。不安全, 但是非常快. */
        template<typename TargetT>
        inline TargetT *reinterpret_get() const {
            return reinterpret_cast<TargetT*>(__ptr);
        }
        /** 使用static转换的版本, 比重解释版本安全一点点, 并且很快 */
        template<typename TargetT>
        requires (std::is_base_of<ObjT, TargetT>::value ||
                  std::is_base_of<TargetT, ObjT>::value)
        inline TargetT *static_get() const {
            return static_cast<TargetT*>(__ptr);
        }
        /** 使用dynamic转换的版本 */
        template<typename TargetT>
        TargetT *dynamic_get() const {
            return dynamic_cast<TargetT*>(__ptr);
        }

        ObjT *operator->() { return __ptr; }
        const ObjT *operator->() const { return __ptr; }

        ObjT &operator*() { return *__ptr; }

        owned &operator=(owned const &ptr) {
            if (__ptr != nullptr) unref();
            if (ptr.__ptr != nullptr) {
                __ptr = ptr.__ptr;
                ref();
            } else
                __ptr = nullptr;
            return *this;
        }
        owned &operator=(owned &ptr) {
            if (__ptr != nullptr) unref();
            if (ptr.__ptr != nullptr)
                __ptr = ptr.ref();
            else
                __ptr = nullptr;
            return *this;
        }
        owned &operator=(owned &&ptr) {
            if (__ptr != nullptr) unref();
            __ptr = ptr.__ptr;
            ptr.__ptr = nullptr;
            return *this;
        }
        owned &operator=(std::nullptr_t) {
            if (__ptr != nullptr) unref();
            __ptr = nullptr;
            return *this;
        }
        template<typename TargetT>
        requires (std::is_base_of<ObjT, TargetT>::value)
        owned &operator=(owned<TargetT> const &ptr) {
            if (__ptr != nullptr) unref();
            __ptr = static_cast<ObjT*>(ptr.__ptr);
            if (__ptr != nullptr)
                ref();
            return *this;
        }
        template<PublicExtendsObject ObjET>
        requires (!std::is_base_of<ObjT, ObjET>::value)
        owned &operator=(owned<ObjET> const &ptr) {
            if (__ptr != nullptr) unref();
            __ptr = dynamic_cast<ObjT*>(ptr.__ptr);
            if (__ptr != nullptr)
                ref();
            return *this;
        }
        template<typename TargetT>
        requires (std::is_base_of<ObjT, TargetT>::value)
        owned &operator=(owned<TargetT> &&ptr) {
            if (__ptr != nullptr) unref();
            __ptr = ptr.__ptr;
            ptr.__ptr = nullptr;
            return *this;
        }

        bool operator==(std::nullptr_t) const { return __ptr == nullptr; }

        /** @fn ref_count() -> int
         * @brief 返回指向的对象的引用计数 */
        int ref_count() { return __ptr->Object::__ref_count__; }
        owned &ref() { __ptr->__ref_count__++; return *this; }
        void ref() const {__ptr->__ref_count__++;}
        void unref() const {
            if (__ptr->__dec_refcnt_and_test())
                delete __ptr;
        }
        void reset() {
            if (__ptr != nullptr)
                unref();
            __ptr = nullptr;
        }
    }; // struct owned

    /** @struct unowned
     * @brief 没有所有权的普通指针, 使用这个模板可以提醒你不要随便乱动这玩意。*/
    template<PublicExtendsObject ObjET>
    using unowned = ObjET*;

    /** @fn make_owned
     * @brief 类似于`std::make_shared`, 这样你就可以使用`auto`来自动推导出`owned`智能指针了.
     * @warning 该函数的参数只能是继承自`Object`的类。 */
    template<PublicExtendsObject ObjT, typename... ArgT>
    inline owned<ObjT> make_owned(ArgT... args)
    {
        ObjT *ret;
        try {
            ret = new ObjT(args...);
        } catch (std::bad_alloc &ae) {
            fputs(ae.what(), stderr);
            fputc('\n', stderr);
            std::terminate();
        } catch (std::exception &e) {
            delete ret;
            throw;
        }
        return owned{ret};
    }
    /** @fn own
     * make_owned的别名 */
    template<PublicExtendsObject ObjT, typename... ArgT>
    inline owned<ObjT> own(ArgT&&... args)
    {
        ObjT *ret;
        try {
            std::allocator<ObjT> allocator;
            ret = allocator.allocate(1);
            new(ret) ObjT(std::forward<ArgT>(args)...);
        } catch (std::bad_alloc &ae) {
            fputs(ae.what(), stderr);
            fputc('\n', stderr);
            std::terminate();
        } catch (std::exception &e) {
            delete ret;
            throw;
        }
        return owned{ret};
    }

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
} // namespace MTB


// 动态指针转换
namespace std {

template<typename DestT, typename SourceT>
requires std::is_base_of<DestT, SourceT>::value || std::is_base_of<SourceT, DestT>::value
MTB::owned<DestT> dynamic_pointer_cast(MTB::owned<SourceT> &&src) {
    MTB::owned<DestT> ret;
    ret.__ptr = dynamic_cast<DestT*>(src.__ptr);
    src.__ptr = nullptr;
    return ret;
}
template<typename DestT, typename SourceT>
requires std::is_base_of<DestT, SourceT>::value || std::is_base_of<SourceT, DestT>::value
MTB::owned<DestT> dynamic_pointer_cast(MTB::owned<SourceT> const& src) {
    MTB::owned<DestT> ret;
    ret.__ptr = dynamic_cast<DestT*>(src.__ptr);
    if (ret.__ptr != nullptr)
        src.__ptr->__ref_count__++;
    return ret;
}

template<typename DestT, typename SourceT>
MTB::owned<DestT> static_pointer_cast(MTB::owned<SourceT> const& src) {
    return MTB::owned<DestT>(static_cast<DestT*>(src));
}

}

// namespace ::
#if __cplusplus > 202002L
using MTB::PublicExtendsObject;
#endif

template<PublicExtendsObject ObjT>
bool operator==(ObjT const* lhs, MTB::owned<ObjT> &&rhs) {
    return lhs == rhs.get();
}
template<PublicExtendsObject ObjT>
bool operator==(MTB::owned<ObjT> const& lhs, MTB::owned<ObjT> &&rhs) {
    return lhs.get() == rhs.get();
}
template<PublicExtendsObject ObjT>
bool operator==(MTB::owned<ObjT> && lhs, ObjT const* const rhs) {
    return lhs.get() == rhs;
}

template<PublicExtendsObject ObjT, PublicExtendsObject ObjET>
requires (!std::is_same_v<ObjET, ObjT>)
bool operator==(MTB::owned<ObjT> const &lhs, MTB::owned<ObjET> &&rhs) {
    return (lhs.__ptr == static_cast<ObjT*>(rhs.__ptr));
}
template<PublicExtendsObject ObjT, PublicExtendsObject ObjET>
requires (!std::is_same_v<ObjET, ObjT>)
bool operator==(ObjT const* lhs, MTB::owned<ObjET> &&rhs) {
    return (lhs == static_cast<ObjT*>(rhs.__ptr));
}
#else
#include "fallback/mtb-object.fallback.hxx"
#endif

#endif
