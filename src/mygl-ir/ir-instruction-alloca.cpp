#include "base/mtb-exception.hxx"
#include "base/mtb-compatibility.hxx"
#include "mygl-ir/ir-basicblock.hxx"
#include "mygl-ir/ir-instruction.hxx"
#include "mygl-ir/irbase-value-visitor.hxx"
#include "ir-instruction-shared.private.hxx"

namespace MYGL::IR {

inline namespace alloca_ssa_impl {
    using RefT = AllocaSSA::RefT;
    using namespace MTB::Compatibility;
    using namespace std::string_literals;
    using namespace std::string_view_literals;

    static size_t get_real_align(size_t align) {
        return get_next_power_of_two(align);
    }

    static PointerType*
    get_pointer_type_or_throw(Type *element_type, SourceLocation &&srcloc)
    {
        MTB_UNLIKELY_IF (element_type == nullptr) {
            throw NullException {
                "AllocaSSA.element_type in construct"sv,
                "", srcloc
            };
        }
        MTB_UNLIKELY_IF (element_type->is_void_type() ||
                            element_type->is_function_type()) {
            throw TypeMismatchException {
                element_type,
                "AllocaSSA.element_type should"
                " be able to make writable instance"s,
                srcloc
            };
        }
        TypeContext *type_ctx = element_type->get_type_context();
        if (type_ctx == nullptr) throw NullException {
            "alloca_ssa_impl(elem_type...)::elem_type.type_ctx"sv,
            osb_fmt("elem type is ", element_type->toString()), CURRENT_SRCLOC_F
        };
        return type_ctx->getPointerType(element_type);
    }

    static inline RefT new_alloca(PointerType *value_type, size_t align) {
        RETURN_MTB_OWN(AllocaSSA, value_type, align);
    }
}

/* ================ [static class AllocaSSA] ================ */
    RefT AllocaSSA::CreateAutoAligned(Type *element_type)
    {
        PointerType *pty = get_pointer_type_or_throw(element_type, CURRENT_SRCLOC_F);
        return new_alloca(pty, element_type->get_instance_align());
    }

    RefT AllocaSSA::CreateWithAlign(TypeContext &ctx, Type *element_type, size_t align)
    {
        PointerType *pty = get_pointer_type_or_throw(element_type, CURRENT_SRCLOC_F);
        return new_alloca(pty, fill_to_power_of_two(align));
    }

/* ================ [dyn class AllocaSSA] ================ */

    AllocaSSA::AllocaSSA(PointerType *value_type, size_t align)
        : Instruction(ValueTID::ALLOCA_SSA, value_type, OpCode::ALLOCA),
          _align(align) {
    }
    AllocaSSA::~AllocaSSA() = default;

    void AllocaSSA::set_align(size_t align)
    {
        _align = get_real_align(align);
    }

    Type *AllocaSSA::get_element_type() const {
        PointerType *value_ptrty = static_cast<PointerType*>(get_value_type());
        return value_ptrty->get_target_type();
    }

    void AllocaSSA::accept(IValueVisitor &visitor) {
        visitor.visit(this);
    }

/* ========== [ AllocaSSA::Signal Handler ] ========== */

    void AllocaSSA::on_parent_plug(BasicBlock *parent)
    {
        Instruction::on_parent_plug(parent);
        _connect_status = ConnectStatus::CONNECTED;
    }

    void AllocaSSA::on_parent_finalize() {
        _connect_status = ConnectStatus::FINALIZED;
    }

    void AllocaSSA::on_function_finalize() {
        _connect_status = ConnectStatus::FINALIZED;
    }
} // namespace MYGL::IR
