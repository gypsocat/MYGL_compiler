#include "base/mtb-compatibility.hxx"
#include "base/mtb-exception.hxx"
#include "mygl-ir/ir-instruction.hxx"
#include "mygl-ir/irbase-value-visitor.hxx"


namespace MYGL::IR {

using namespace std::string_view_literals;
using namespace Compatibility;

/** =============== [ @class ExtractElemSSA ] ================ */
/* ================ [ static class ExtractElemSSA ] ================ */

ExtractElemSSA::RefT
ExtractElemSSA::Create(owned<Value> array, owned<Value> index)
{
    std::string_view nullref{};
    if (array == nullptr)
        nullref = "ExtractElemSSA::Create()::array"sv;
    else if (index == nullptr)
        nullref = "ExtractElemSSA::Create()::index"sv;
    if (!nullref.empty()) throw NullException {
        nullref, {}, CURRENT_SRCLOC_F
    };
    Type *arrayty = array->get_value_type();
    Type *indexty = index->get_value_type();
    if (!arrayty->is_array_type()) throw TypeMismatchException {
        arrayty, rt_fmt("type %p should "
        "be array type, but it is %s now", arrayty, MTB_STR_FMT(arrayty->toString())),
        CURRENT_SRCLOC_F
    };
    if (!indexty->is_integer_type()) throw TypeMismatchException {
        indexty,
        rt_fmt("type %p should be integer type, while it is %s",
            indexty, MTB_STR_FMT(indexty->toString())),
        CURRENT_SRCLOC_F
    };
    auto *arr_aty = static_cast<ArrayType*>(arrayty);
    RETURN_MTB_OWN(ExtractElemSSA, std::move(array), std::move(index),
                   arr_aty, arr_aty->get_element_type());
}

ExtractElemSSA::RefT ExtractElemSSA::
CreateFromComptimeIndex(owned<Value> array, IntType *ity, uint32_t index)
{
    if (ity == nullptr) throw NullException {
        "ExtractElemSSA::Create...()::ity"sv,
        {}, CURRENT_SRCLOC_F
    };
    auto vidx = own<IntConst>(ity, index);
    return Create(std::move(array), std::move(vidx));
}
/* ================ [ public class ExtractElemSSA ] ================ */

ExtractElemSSA::ExtractElemSSA(owned<Value> array, owned<Value> index,
                               ArrayType *arr_type, Type *elem_type)
    : Instruction(ValueTID::EXTARCT_ELEM_SSA, elem_type,
                  OpCode::EXTRACT_ELEMENT),
      _array_type(arr_type),
      _array(std::move(array)),
      _index(std::move(index)) {
    MYGL_ADD_USEE_PROPERTY(array);
    MYGL_ADD_USEE_PROPERTY(index);
}
ExtractElemSSA::~ExtractElemSSA() = default;

void ExtractElemSSA::set_array(owned<Value> array)
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

void ExtractElemSSA::set_index(owned<Value> index)
{
    if (_index == index)
        return;
    Use *idxuse = _list_as_user.at(1);
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

void ExtractElemSSA::accept(IValueVisitor &visitor) {
    visitor.visit(this);
}
Value *ExtractElemSSA::operator[](size_t index)
{
    if (index == 0)
        return _array.get();
    else if (index == 1)
        return _index.get();
    return nullptr;
}
void ExtractElemSSA::traverseOperands(ValueAccessFunc fn)
{
    if (fn(*_array.get()))
        return;
    if (fn(*_index.get()))
        return;
}
/* ================ [ public signal ExtractElemSSA ] ================ */

void ExtractElemSSA::on_parent_plug(BasicBlock *parent)
{
    _parent = parent;
    _connect_status = Instruction::ConnectStatus::CONNECTED;
}
void ExtractElemSSA::on_parent_finalize()
{
    auto use_it = _list_as_user.begin();
    Use *arr_use = use_it.get();
    if (_array != nullptr) {
        _array->removeUseAsUsee(arr_use);
        _array.reset();
    }
    ++use_it;
    Use *idx_use = use_it.get();
    if (_index != nullptr) {
        _index->removeUseAsUsee(idx_use);
        _index.reset();
    }
    _connect_status = Instruction::ConnectStatus::FINALIZED;
}
void ExtractElemSSA::on_function_finalize()
{
    auto use_it = _list_as_user.begin();
    Use *arr_use = use_it.get();
    if (_array != nullptr) {
        _array->removeUseAsUsee(arr_use);
        _array.reset();
    }
    ++use_it;
    Use *idx_use = use_it.get();
    if (_index != nullptr) {
        _index->removeUseAsUsee(idx_use);
        _index.reset();
    }
    _connect_status = Instruction::ConnectStatus::FINALIZED;
}

/* ================ [ end class ExtractElemSSA ] ================ */
} // namespace MYGL::IR
