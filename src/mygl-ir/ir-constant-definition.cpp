#include "base/mtb-compatibility.hxx"
#include "base/mtb-exception.hxx"
#include "mygl-ir/ir-constant.hxx"
#include "mygl-ir/ir-module.hxx"
#include "mygl-ir/irbase-value-visitor.hxx"
#include <iostream>

namespace MYGL::IR {

using namespace std::string_literals;
using namespace std::string_view_literals;

/** class Definition */
/* ================ [public class Definition] ================ */
Definition::Definition(ValueTID type_id, PointerType *value_type, Module *parent)
    : Constant(ValueTID::DEFINITION, value_type),
      _parent(parent) {
}
Definition::~Definition() = default;

/** class GlobalVariable */
inline namespace gvar_impl {
    static void chk_nonnull_2(Module *module, Type *argtype2,
                              SourceLocation srcloc) noexcept(false)
    {
        if (module == nullptr) throw NullException {
            "GlobalVariable::Create...().module"sv,
            {}, srcloc
        };
        if (argtype2 == nullptr) throw NullException {
            "GlobalVariable::Create...().arg[2]"sv,
            {}, srcloc
        };
    }
    static void chk_nonnull_2_1(Module *module, Value *value,
                                SourceLocation srcloc) noexcept(false)
    {
        if (module == nullptr) throw NullException {
            "GlobalVariable::Create...().module"sv,
            {}, srcloc
        };
        if (value == nullptr) throw NullException {
            "GlobalVariable::Create...().value"sv,
            {}, srcloc
        };
    }

    static always_inline GlobalVariable::RefT
    new_gvar(Module *parent,    PointerType *gvty,
             owned<Value> ival, bool target_mutable) {
        RETURN_MTB_OWN(GlobalVariable, parent, gvty,
                       std::move(ival), target_mutable);
    }
} // inline namespace gvar_impl
/* ================ [static class GlobalVariable] ================ */
GlobalVariable::RefT GlobalVariable::
CreateExternRaw(Module *parent, PointerType *gvty, bool t_mut)
{
    chk_nonnull_2(parent, gvty, CURRENT_SRCLOC_F);
    if (gvty->get_target_type()->is_void_type()) {
        throw TypeMismatchException {
            gvty,
            "GlobalVariable pointer type should"
            "not be void pointer"s, CURRENT_SRCLOC_F
        };
    }
    return new_gvar(parent, gvty, nullptr, t_mut);
}
GlobalVariable::RefT GlobalVariable::
CreateExtern(Module *parent, Type *elemty, bool t_mut)
{
    chk_nonnull_2(parent, elemty, CURRENT_SRCLOC_F);
    if (elemty->is_void_type()) {
        throw TypeMismatchException {
            elemty,
            "GlobalVariable element type should"
            "not be void"s, CURRENT_SRCLOC_F
        };
    }
    PointerType *pty = elemty->get_type_context()->getPointerType(elemty);
    return new_gvar(parent, pty, nullptr, t_mut);
}

GlobalVariable::RefT GlobalVariable::
CreateDefaultRaw(Module *parent, PointerType *gvty, bool t_mut)
{
    chk_nonnull_2(parent, gvty, CURRENT_SRCLOC_F);
    Type *elemty = gvty->get_target_type();
    if (elemty->is_void_type()) {
        throw TypeMismatchException {
            gvty,
            "GlobalVariable pointer type should"
            "not be void pointer"s, CURRENT_SRCLOC_F
        };
    }
    auto default_value = CreateZeroOrUndefined(elemty);
    return new_gvar(parent, gvty, default_value, t_mut);
}
GlobalVariable::RefT GlobalVariable::
CreateDefault(Module *parent, Type *elemty, bool t_mut)
{
    chk_nonnull_2(parent, elemty, CURRENT_SRCLOC_F);
    if (elemty->is_void_type()) {
        throw TypeMismatchException {
            elemty,
            "GlobalVariable element type should"
            "not be void"s, CURRENT_SRCLOC_F
        };
    }
    PointerType *pty = elemty->get_type_context()->getPointerType(elemty);
    auto default_value = CreateZeroOrUndefined(elemty);
    return new_gvar(parent, pty, nullptr, t_mut);
}

GlobalVariable::RefT GlobalVariable::
CreateWithValue(Module *parent, owned<Constant> init_value, bool tm)
{
    chk_nonnull_2_1(parent, init_value, CURRENT_SRCLOC_F);
    Type      *elemty = init_value->get_value_type();
    TypeContext *tctx = elemty->get_type_context();
    PointerType *gvty = tctx->getPointerType(elemty);
    return new_gvar(parent, gvty, std::move(init_value), tm);
}
/* ================ [public class GlobalVariable] ================ */
GlobalVariable::GlobalVariable(Module *parent, PointerType  *gvar_type,
                          owned<Constant> init_value, bool target_mutable) noexcept
    : Definition(ValueTID::GLOBAL_VARIABLE, gvar_type, parent),
      _target_mutable(target_mutable),
      _target(std::move(init_value)) {
    _align = gvar_type->get_target_type()->get_instance_align();
    MYGL_ADD_TYPED_USEE_PROPERTY(Constant, target);
}
GlobalVariable::~GlobalVariable() = default;

void GlobalVariable::set_target(owned<Constant> target)
try {
    if (target == _target)
        return;
    Use *use = _list_as_user.front();
    if (_target != nullptr)
        _target->removeUseAsUsee(use);
    if (target != nullptr)
        target->addUseAsUsee(use);
} catch (EmptySetException &e) {
    std::cerr << std::endl
              << Compatibility::osb_fmt("at GlobalVariable::set_target(owned<Constant> target)"
                         "(BasicBlock *from), line 96") << std::endl;
        throw e;
}
void GlobalVariable::accept(IValueVisitor &visitor) {
    visitor.visit(this);
}
Type *GlobalVariable::get_target_type() const {
    return get_value_ptr_type()->get_target_type();
}

} // namespace MYGL::IR
