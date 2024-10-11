#include "mygl-ir/ir-constant.hxx"
#include "mygl-ir/irbase-value-visitor.hxx"

inline namespace constexpr_impl {

    using namespace MYGL::IR;

    static ArrayType *get_value_array_type(Value::ValueArrayT const &arr)
    {
        if (arr.empty()) {
            throw EmptySetException {
                "Value Array from ArrayExpr(vector<Value>)",
                "Value array cannot be empty or the constructor cannot get the target type."
            };
        }
        Type  *elem_type = arr[0]->get_value_type();
        TypeContext &ctx = *elem_type->get_type_context();
        size_t    length = arr.size();
        return ctx.getArrayType(elem_type, length);
    }

    static inline void
    init_element_array(ConstantExpr::ValueArrayT &element_array, ArrayType *array_type)
    {
        element_array.resize(array_type->get_length());
        Type *elemty = array_type->get_element_type();
        if (elemty->is_array_type()) {
            for (owned<Value> &i: element_array)
                i = ArrayExpr::CreateEmpty(static_cast<ArrayType*>(elemty));
        } else {
            owned<Value> zero = Constant::CreateZeroOrUndefined(elemty);
            for (owned<Value> &i: element_array)
                i = zero;
        }
    }

} // namespace constexpr_impl

namespace MYGL::IR {

/** @class ConstantExpr */
uint32_t ConstantExpr::get_content_nmemb() const
{
    Type *value_type = get_value_type();
    switch (value_type->get_type_id()) {
    case TypeTID::POINTER_TYPE:
    case TypeTID::LABEL_TYPE:
        return 1;
    case TypeTID::ARRAY_TYPE:
        return static_cast<ArrayType*>(value_type)->get_length();
    default: return 0;
    }
}

Type *ConstantExpr::contentTypeAt(size_t index) const
{
    Type *value_type = _value_type;
    return value_type->get_base_type();
}

ConstantExpr::ConstantExpr(ValueTID type_id, Type *value_type)
    : Constant(type_id, value_type) {
}
/** end class ConstantExpr */

/** @class ArrayExpr */
owned<ArrayExpr> ArrayExpr::
CreateFromValueArray(ValueArrayT const& value_array, Type *content_type)
{
    if (value_array.empty() &&
        content_type == nullptr) throw NullException {
            "ArrayExpr::content_type",
            "Cannot get array type from empty value array and null"
                "array type",
            CURRENT_SRCLOC_F
    };

    Value *val = nullptr;
    for (uint64_t i = 0; i < value_array.size(); i++) {
        val = value_array[i];
        if (val != nullptr)
            break;
    }
    if (val != nullptr)
        content_type = val->get_value_type();
    if (content_type == nullptr) throw NullException {
            "ArrayExpr::content_type",
            "Cannot get array type from empty value array and null"
                "array type",
            CURRENT_SRCLOC_F
    };
    ArrayType *arrty = content_type->get_type_context()
                                   ->getArrayType(content_type,
                                                  value_array.size());
    owned<ArrayExpr> ret = own<ArrayExpr>(arrty);
    ret->element_list()  = value_array;
    return ret;
}
/** public class ArrayExpr */
ArrayExpr::ArrayExpr(ArrayType *array_type)
    : ConstantExpr(ValueTID::ARRAY, array_type) {
    size_t nmemb = array_type->get_length();
    _element_list.resize(0);
}
ArrayExpr::ArrayExpr(ValueArrayT &&value_array)
    : ConstantExpr(ValueTID::ARRAY, get_value_array_type(value_array)),
      _element_list(std::move(value_array)) {
}

ArrayExpr::ValueArrayT &ArrayExpr::element_list()
{
    MTB_LIKELY_IF (!_element_list.empty())
        return _element_list;

    ArrayType *array_type = static_cast<ArrayType*>(get_value_type());
    init_element_array(_element_list, array_type);
    return _element_list;
}

ArrayExpr::ValueArrayT const&
ArrayExpr::get_element_list() const
{
    MTB_LIKELY_IF (!_element_list.empty())
        return _element_list;
    
    ArrayType *array_type = static_cast<ArrayType*>(get_value_type());
    init_element_array(const_cast<ValueArrayT&>(_element_list), array_type);
    return _element_list;
}

owned<Value> ArrayExpr::getContent(size_t index)
{
    if (index < 0 || index >= get_content_nmemb()) {
        return new UndefinedConst(get_content_type()); 
    }
    Value *value = element_list()[index];
    if (ArrayType *aty = dynamic_cast<ArrayType*>(value);
        !value->is_defined() && aty != nullptr) {
        Value *value_expend = new ArrayExpr(aty);
        _element_list[index] = value_expend;
        return value_expend;
    }
    return value;
}
bool ArrayExpr::setContent(size_t index, owned<Value> content)
{
    if (!content->get_value_type()->equals(get_content_type()))
        return false;
    if (_element_list.empty()) {
        ArrayType *arrty = static_cast<ArrayType*>(_value_type);
        _element_list.resize(arrty->get_length());
    }
    _element_list[index] = content;
    return true;
}

void ArrayExpr::traverseContent(ValueReadFunc fn)
{
    Type *elem_type = static_cast<ArrayType*>(_value_type)->get_element_type();
    bool  elem_type_is_array = elem_type->is_array_type();
    for (Value *i: element_list()) {
        if (fn(i))
            break; 
    }
}
void ArrayExpr::traverseContent(ValueReadClosure fn)
{
    Type *elem_type = static_cast<ArrayType*>(_value_type)->get_element_type();
    bool  elem_type_is_array = elem_type->is_array_type();
    for (auto &i : element_list()) {
        if (!i->is_defined() && elem_type_is_array)
            i = ArrayExpr::CreateEmpty(static_cast<ArrayType*>(elem_type));
        if (fn(i)) break; 
    }
}

void ArrayExpr::traverseModify(ValueModifyClosure fn)
{
    Type *elem_type = static_cast<ArrayType*>(_value_type)->get_element_type();
    bool  elem_type_is_array = elem_type->is_array_type();
    for (auto &i : element_list()) {
        Value *new_i = nullptr;
        if (!i->is_defined() && elem_type_is_array)
            i = ArrayExpr::CreateEmpty(static_cast<ArrayType*>(elem_type));
        if (fn(i, new_i)) break;
        if (new_i == i || new_i == nullptr)
            continue;
        i = new_i; 
    }
}

bool ArrayExpr::is_zero() const
{
    bool ret = true;
    if (_element_list.empty())
        return true;
    for (Value *i: _element_list) {
        Constant *c = dynamic_cast<Constant*>(i);
        if (c == nullptr)
            return false;
        ret &= c->is_zero();
    }
    return ret;
}

void ArrayExpr::reset() {
    _element_list.clear();
}
void ArrayExpr::accept(IValueVisitor &visitor) {
    visitor.visit(this);
}

/** end class ArrayExpr */

} // MYGL::IR