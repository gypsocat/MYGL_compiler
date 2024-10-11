#include "mygl-ir/irbase-type-context.hxx"
#include <cstdio>

#define ity_map _optimized_int_type_map

using FTypeListT = MYGL::IRBase::TypeContext::FTypeListT;

namespace MYGL::IRBase {

/** @class TypeContext */
TypeContext::TypeContext(size_t machine_word_size)
    : _machine_word_size(machine_word_size) {
    registerTypes(VoidType::voidty);
    registerTypes(IntType::i0,
                  IntType::i1,  IntType::i8,
                  IntType::i32, IntType::i64);
    ity_map.insert({0,  IntType::i0});
    ity_map.insert({1,  IntType::i1});
    ity_map.insert({8,  IntType::i8});
    ity_map.insert({32, IntType::i32});
    ity_map.insert({64, IntType::i64});
    
    registerTypes(FloatType::ieee_f32,
                  FloatType::ieee_f64);
    registerTypes(LabelType::labelty);
}

bool TypeContext::hasType(Type *type) const {
    return _type_set.contains(type);
}
Type *TypeContext::getOrRegisterType(Type *type)
{
    if (hasType(type))
        return _type_set.find(type)->get();
    _type_set.insert(type);
    if (type->get_type_context() == nullptr)
        type->set_type_context(this);
    return type;
} 
Type* TypeContext::getOrRegisterType(owned<Type> type)
{
    if (hasType(type))
        return _type_set.find(type)->get();
    Type *raw_ty = type;
    _type_set.insert(std::move(type));
    // 针对IntType做特殊加速处理
    if (type->get_type_id() == TypeTID::INT_TYPE) {
        IntType *ity = static_cast<IntType*>(type.get());
        ity_map.insert({ity->get_binary_bits(), ity});
    }
    if (raw_ty->get_type_context() == nullptr)
        raw_ty->set_type_context(this);
    return raw_ty;
}

IntType *TypeContext::getIntType(size_t binary_bits, bool is_unsigned)
{
    if (auto iter = ity_map.find(binary_bits);
        iter != ity_map.end())
        return iter->second.get();

    owned<IntType> ity = own<IntType>(binary_bits, is_unsigned);
    IntType *ret = ity.get();
    _type_set.insert(ity);
    ity_map.insert({binary_bits, ity});
    return ret;
}

FloatType *TypeContext::getIeeeF32() {
    return FloatType::ieee_f32;
}
FloatType *TypeContext::getIeeeF64() {
    return FloatType::ieee_f64;
}
FunctionType *TypeContext::getFunctionType(
                            Type *return_type,
                            FTypeListT const &param_list)
{
    Type *rret_ty = getOrRegisterType(return_type);
    auto fty = own<FunctionType>(rret_ty, param_list);
    fty->set_type_context(this);
    return static_cast<FunctionType*>(getOrRegisterType(fty.get()));
}
FunctionType *TypeContext::getFunctionType(
                            Type *return_type,
                            FTypeListT &&param_list)
{
    Type *rret_ty = getOrRegisterType(return_type);
    auto fty = own<FunctionType>(rret_ty, std::move(param_list));
    fty->set_type_context(this);
    return static_cast<FunctionType*>(getOrRegisterType(fty.get()));
}
PointerType *TypeContext::getFunctionPointer(
                                    owned<Type> return_type,
                                    FTypeListT const &param_list)
{
    Type *fty = getFunctionType(return_type, param_list);
    if (fty == nullptr) throw NullException {
        "TypeContext::getFunctionPointer()::...fty",
        "Function type is NULL. What's wrong?",
        CURRENT_SRCLOC_F
    }; // fty == nullptr
    return getPointerType(fty);
}

ArrayType *TypeContext::getArrayType(Type *element_type, size_t length)
{
    element_type = getOrRegisterType(element_type);
    owned<ArrayType> arrty = new ArrayType(*this, element_type, length);
    Type *ret = getOrRegisterType(std::move(arrty).static_get<Type>());
    return static_cast<ArrayType*>(ret);
}
ArrayType *TypeContext::getMultiDimensionArray(Type *element_type,
                                               AIndexListT const &length_list)
{
    owned<Type> ret{getOrRegisterType(element_type)};
    for (auto i = length_list.rbegin();
         i != length_list.rend(); i--) {
        size_t leng = *i;
        if (leng == 0)
            return nullptr;
        ret = getArrayType(ret, leng);
    }
    return ret.dynamic_get<ArrayType>();
}

PointerType *TypeContext::getPointerType(Type *elemty, bool is_const)
{
    elemty = getOrRegisterType(elemty);
    if (elemty == nullptr) throw NullException {
        "TypeContext::getFunctionPointer()::...elemty",
        "Registeded element type is NULL. What's wrong?",
        CURRENT_SRCLOC_F
    }; // fty == nullptr

    auto pty = own<PointerType>(elemty, is_const);
    pty->set_type_context(this);
    Type *ret = getOrRegisterType((Type*)pty);

    if (ret == nullptr) throw NullException {
        "TypeContext::getFunctionPointer()::...ret",
        "Returned element type is NULL. What's wrong?",
        CURRENT_SRCLOC_F
    }; // fty == nullptr
    return pty.get();
    // return makeType<PointerType>(elemty, is_const);
}
/** end class TypeContext */

} // namespace MYGL::IRBase
