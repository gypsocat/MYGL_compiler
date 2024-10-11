#ifndef __MYGL_IRBASE_TYPE_CONTEXT_H__
#define __MYGL_IRBASE_TYPE_CONTEXT_H__

#include "base/mtb-getter-setter.hxx"
#include "base/mtb-object.hxx"
#include "irbase-type.hxx"
#include <cstddef>
#include <deque>
#include <unordered_map>
#include <unordered_set>

namespace MYGL::IRBase {

/** @class TypeContext
 * @brief 一个简单的类型管理器，可以内嵌在Module里做类型管理，也可以全局使用。 */
class TypeContext {
public:
    using TypePtrT = owned<Type>;
    using TypeSet  = std::unordered_set<TypePtrT, TypeHasher, TypeFullEqual>;
    using FTypeListT  = FunctionType::TypeListT;
    using AIndexListT = std::deque<size_t>;

    using IntTypeMapT = std::unordered_map<size_t, owned<IntType>>;
public:
    explicit TypeContext(size_t machine_word_size = global_machine_word_size);
    // explicit TypeContext(TypeContext const &tc)
    //     : _machine_word_size(tc._machine_word_size),
    //       _type_set(tc._type_set),
    //       _optimized_int_type_map(tc._optimized_int_type_map) {}
    // explicit TypeContext(TypeContext &&tc)
    //     : _machine_word_size(tc._machine_word_size),
    //       _type_set(std::move(tc._type_set)){}

    DEFINE_ACCESSOR(TypeSet, type_set)
    DECLARE_TYPED_GETTER(TypeSet const &, type_set)

    DEFINE_GETSET(size_t, machine_word_size)

    bool hasType(Type *type) const;
    [[nodiscard("TypeContext.getOrRegisterType(type)->registered type")]]
    Type* getOrRegisterType(Type *type);
    [[nodiscard("TypeContext.getOrRegisterType(type)->registered type")]]
    Type* getOrRegisterType(owned<Type> ty);

    template<typename TypeT, typename...Args>
    MTB_REQUIRE((std::is_base_of<Type, TypeT>::value))
    TypeT *makeType(Args&...args) {
        owned<Type> target = own<TypeT>(args...);
        target->set_type_context(this);
        Type *ret = getOrRegisterType(target.get());

        if (ret == nullptr) throw NullException {
            "TypeContext::makeType<...>()::...ret",
            "Returned element type is NULL. What's wrong?",
            CURRENT_SRCLOC_F
        }; // fty == nullptr
        return static_cast<TypeT*>(ret);
    }

    size_t registerTypes(Type *type) {
        type->set_type_context(this);
        return getOrRegisterType(type) == type;
    }
    template<typename...Types>
    size_t registerTypes(Type *type, Types...types) {
        size_t ret = registerTypes(type);
        ret += registerTypes(types...);
        return ret;
    }
    
    IntType   *getIntType(size_t binary_bits, bool is_unsigned = false);
    FloatType *getIeeeF32();
    FloatType *getIeeeF64();

    FunctionType *getFunctionType(Type* return_type,
                                  FTypeListT const &param_list);
    FunctionType *getFunctionType(Type* return_type,
                                  FTypeListT &&param_list);
    PointerType  *getFunctionPointer(owned<Type> return_type,
                                     FTypeListT const &param_list);

    ArrayType *getArrayType(Type *element_type, size_t length);
    ArrayType *getMultiDimensionArray(Type *element_type, AIndexListT const &length_list);

    ArrayType *getMultiDimensionArray(Type *element_type, size_t length0) {
        return getArrayType(element_type, length0);
    }
    template<typename...IndexArgs>
    ArrayType *getMultiDimensionArray(Type *element_type, size_t length0, IndexArgs...args)
    {
        ArrayType *ret_elem = getMultiDimensionArray(element_type, args...);
        if (ret_elem == nullptr)
            return nullptr;
        return getArrayType(ret_elem, length0);
    }

    PointerType  *getPointerType(Type *target_type, bool is_constant = false);
private:
    TypeSet     _type_set;
    IntTypeMapT _optimized_int_type_map;
    size_t  _machine_word_size;
}; // class TypeContext

} // namespace MYGL::IRBase

#endif
