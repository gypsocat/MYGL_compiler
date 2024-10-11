#include "base/mtb-compatibility.hxx"
#include "base/mtb-exception.hxx"
#include "mygl-ir/ir-instruction.hxx"
#include "mygl-ir/irbase-value-visitor.hxx"

namespace MYGL::IR {

using namespace std::string_view_literals;
using namespace Compatibility;

/** =============== [ @class InsertElemSSA ] ================ */
/* ================ [ static class InsertElemSSA ] ================ */

InsertElemSSA::RefT
InsertElemSSA::Create(owned<Value> array, owned<Value> element, owned<Value> index)
{
    std::string_view nullref{};
    if (array == nullptr)
        nullref = "InsertElemSSA::Create()::array"sv;
    else if (index == nullptr)
        nullref = "InsertElemSSA::Create()::index"sv;
    else if (element == nullptr)
        nullref = "InsertElemSSA::Create()::element"sv;
    if (!nullref.empty()) throw NullException {
        nullref, {}, CURRENT_SRCLOC_F
    };

    Type *arrayty = array->get_value_type();
    Type *elem_ty = element->get_value_type();
    Type *indexty = index->get_value_type();
    if (!arrayty->is_array_type()) throw TypeMismatchException {
        arrayty, rt_fmt("type %p should "
        "be array type, but it is %s now", arrayty, MTB_STR_FMT(arrayty->toString())),
        CURRENT_SRCLOC_F
    };
    auto *arr_aty = static_cast<ArrayType*>(arrayty);
    Type *elemrty = arr_aty->get_element_type();
    if (elem_ty != arr_aty && !elem_ty->equals(arr_aty)) {
        throw TypeMismatchException {
            elem_ty, "Expected element type of `array` type",
            CURRENT_SRCLOC_F
        };
    }
    if (!indexty->is_integer_type()) throw TypeMismatchException {
        indexty,
        rt_fmt("type %p should be integer type, while it is %s",
            indexty, MTB_STR_FMT(indexty->toString())),
        CURRENT_SRCLOC_F
    };
    RETURN_MTB_OWN(InsertElemSSA, std::move(array), std::move(element), std::move(index));
}
/* ================ [ public class InsertElemSSA ] ================ */

InsertElemSSA::InsertElemSSA(owned<Value> array, owned<Value> element,
                             owned<Value> index)
    : Instruction(ValueTID::INSERT_ELEM_SSA, array->get_value_type(),
                  OpCode::INSERT_ELEMENT),
      _array_type(static_cast<ArrayType*>(array->get_value_type())),
      _array(std::move(array)), _element(std::move(element)),
      _index(std::move(index)) {
    MYGL_ADD_USEE_PROPERTY(array);
    MYGL_ADD_USEE_PROPERTY(element);
    MYGL_ADD_USEE_PROPERTY(index);
}
InsertElemSSA::~InsertElemSSA() = default;

void InsertElemSSA::set_array(owned<Value> array)
{
    if (_array == array)
        return;
    Use *arruse = _list_as_user.front();
    if (_array != nullptr)
        _array->removeUseAsUsee(arruse);
    if (array == nullptr) {
        _array.reset();
        return;
    }

    if (Type *new_arrty = array->get_value_type();
        new_arrty != _array_type && !new_arrty->equals(_array_type)) {
        throw TypeMismatchException {
            new_arrty,
            rt_fmt("Type %p should be the same as %s, but it is %s",
                new_arrty,
                MTB_STR_FMT(_array_type->toString()),
                MTB_STR_FMT(new_arrty->toString())),
            CURRENT_SRCLOC_F
        };
    }
    array->addUseAsUsee(arruse);
    _array = std::move(array);
}

void InsertElemSSA::set_element(owned<Value> element)
{
    if (_element == element)
        return;
    Use *idxuse = _list_as_user.at(1);
    if (_element != nullptr)
        _element->removeUseAsUsee(idxuse);
    if (element == nullptr) {
        _element.reset();
        return;
    }
    if (Type *idxty = element->get_value_type();
        idxty != _array_type->get_element_type() &&
        !idxty->equals(_array_type->get_element_type()))
        throw TypeMismatchException {
        idxty,
        rt_fmt("Expected integer type, but %s",
               MTB_STR_FMT(idxty->toString())),
        CURRENT_SRCLOC_F
    };
    element->addUseAsUsee(idxuse);
    _element = std::move(element);
}

void InsertElemSSA::set_index(owned<Value> index)
{
    if (_index == index)
        return;
    Use *idxuse = _list_as_user.at(2);
    if (_index != nullptr)
        _index->removeUseAsUsee(idxuse);
    if (index == nullptr) {
        _index.reset();
        return;
    }
    if (Type *idxty = index->get_value_type();
        !idxty->is_integer_type()) throw TypeMismatchException {
        idxty,
        rt_fmt("Expected integer type, but %s",
               MTB_STR_FMT(idxty->toString())),
        CURRENT_SRCLOC_F
    };
    index->addUseAsUsee(idxuse);
    _index = std::move(index);
}

void InsertElemSSA::accept(IValueVisitor &visitor) {
    visitor.visit(this);
}
Value *InsertElemSSA::operator[](size_t index)
{
    if (index == 0)
        return _array.get();
    else if (index == 1)
        return _element.get();
    else if (index == 2)
        return _index.get();
    return nullptr;
}
void InsertElemSSA::traverseOperands(ValueAccessFunc fn)
{
    if (fn(*_array.get()))
        return;
    if (fn(*_element.get()))
        return;
    if (fn(*_index.get()))
        return;
}
/* ================ [ public signal InsertElemSSA ] ================ */

void InsertElemSSA::on_parent_plug(BasicBlock *parent)
{
    _parent = parent;
    _connect_status = Instruction::ConnectStatus::CONNECTED;
}
void InsertElemSSA::on_parent_finalize()
{
    auto use_it = _list_as_user.begin();
    if (_array != nullptr) {
        Use *arr_use = use_it.get();
        _array->removeUseAsUsee(arr_use);
        _array.reset();
    }
    ++use_it;
    if (_element != nullptr) {
        Use *elm_use = use_it.get();
        _element->removeUseAsUsee(elm_use);
        _element.reset();
    }
    ++use_it;
    if (_index != nullptr) {
        Use *idx_use = use_it.get();
        _index->removeUseAsUsee(idx_use);
        _index.reset();
    }
    _connect_status = Instruction::ConnectStatus::FINALIZED;
}
void InsertElemSSA::on_function_finalize()
{
    auto use_it = _list_as_user.begin();
    if (_array != nullptr) {
        Use *arr_use = use_it.get();
        _array->removeUseAsUsee(arr_use);
        _array.reset();
    }
    ++use_it;
    if (_element != nullptr) {
        Use *elm_use = use_it.get();
        _element->removeUseAsUsee(elm_use);
        _element.reset();
    }
    ++use_it;
    if (_index != nullptr) {
        Use *idx_use = use_it.get();
        _index->removeUseAsUsee(idx_use);
        _index.reset();
    }
    _connect_status = Instruction::ConnectStatus::FINALIZED;
}

/* ================ [ end class InsertElemSSA ] ================ */
} // namespace MYGL::IR
