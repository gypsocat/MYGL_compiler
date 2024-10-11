#include "base/mtb-exception.hxx"
#include "mygl-ir/ir-constant.hxx"
#include "mygl-ir/ir-instruction.hxx"
#include "mygl-ir/irbase-value-visitor.hxx"

namespace MYGL::IR {
    using RefT = StoreSSA::RefT;

inline namespace store_ssa_impl {
    using namespace std::string_view_literals;
    using namespace std::string_literals;
    using namespace MTB::Compatibility;

    enum OperandOrder: uint8_t {
        SOURCE = 0, TARGET = 1
    }; // enum OperandOrder

    static PointerType *
    get_target_type_or_throw(Value *target, SourceLocation &&srcloc = CURRENT_SRCLOC)
    {
        Type *target_type = target->get_value_type();
        if (!target_type->is_pointer_type()) {
            throw TypeMismatchException {
                target_type,
                "StoreSSA target type should be pointer type"s,
                srcloc
            };
        }
        return static_cast<PointerType*>(target_type);
    }

    static void
    check_source_type_or_throw(Value *source, PointerType *target_type,
                               SourceLocation &&srcloc = CURRENT_SRCLOC)
    {
        Type *source_ty = source->get_value_type();
        Type *elem_type = target_type->get_target_type();
        if (source_ty != elem_type && !source_ty->equals(elem_type)) {
            throw TypeMismatchException { source_ty,
                osb_fmt("StoreSSA source type should be"
                " the target of target pointer type "sv, target_type->toString()),
                srcloc
            };
        }
    }

    static PointerType*
    construct_check_get_target_type(Value *source, Value *target,
                                    SourceLocation &&srcloc = CURRENT_SRCLOC)
    {
        if (target == nullptr) {
            throw NullException {
                "StoreSSA.target in construct"sv,
                ""s, srcloc
            };
        }
        if (source == nullptr) {
            throw NullException {
                "StoreSSA.source in construct"sv,
                ""s,srcloc
            };
        }
        PointerType *ret_target_pty = get_target_type_or_throw(target, std::move(srcloc));
        check_source_type_or_throw(source, ret_target_pty, std::move(srcloc));
        return ret_target_pty;
    }

    template<typename PointerValueT>
    MTB_REQUIRE((std::is_base_of_v<Value, PointerValueT>))
    struct AlignGetter {
        PointerValueT *ptr_value;
        always_inline size_t get_align() const {
            return ptr_value->get_align();
        }
    }; // struct AlignGetter<PointerValueT>

    template<typename PointerValueT, typename AlignGetterT = AlignGetter<PointerValueT>>
    MTB_REQUIRE((std::is_base_of_v<Value, PointerValueT>))
    static always_inline RefT
    create_store_from_pointer(owned<Value> source, PointerValueT *target, SourceLocation &&srcloc)
    {
        if (target == nullptr) {
            throw NullException {
                "StoreSSA.target in construct",
                ""s, srcloc
            };
        }

        if (source == nullptr) {
            throw NullException {
                "StoreSSA.source in construct"sv,
                ""s, srcloc
            };
        }

        PointerType *target_pointer_type = static_cast<PointerType*>(target->get_value_type());
        check_source_type_or_throw(source, target_pointer_type);
        size_t align = AlignGetterT{target}.get_align();

        RETURN_MTB_OWN(StoreSSA, std::move(source), target, target_pointer_type, align);
    }
} // inline namespace store_ssa_impl

/* ================ [static class StoreSSA] ================ */

    RefT StoreSSA::Create(owned<Value> source, owned<Value> target)
    {
        PointerType *target_ty = construct_check_get_target_type(source, target, CURRENT_SRCLOC_F);
        Type        *source_ty = target_ty->get_target_type();
        size_t align = source_ty->get_instance_align();
        RETURN_MTB_OWN(StoreSSA, std::move(source), std::move(target), target_ty, align);
    }

    RefT StoreSSA::CreateWithAlign(owned<Value> source, owned<Value> target, size_t align)
    {
        PointerType *target_ty = construct_check_get_target_type(source, target, CURRENT_SRCLOC_F);
        Type        *source_ty = target_ty->get_target_type();
        RETURN_MTB_OWN(StoreSSA, std::move(source), std::move(target), target_ty, align);
    }

    RefT StoreSSA::CreateFromAlloca(owned<Value> source, AllocaSSA *alloca_ssa) {
        return create_store_from_pointer(std::move(source), alloca_ssa, CURRENT_SRCLOC_F);
    }

    RefT StoreSSA::CreateFromGlobalVariable(owned<Value> source, GlobalVariable *gvar) {
        return create_store_from_pointer(std::move(source), gvar, CURRENT_SRCLOC_F);
    }
/* ================ [public class StoreSSA] ================ */
    StoreSSA::StoreSSA(owned<Value> source, owned<Value> target,
                       PointerType *target_pointer_type, size_t align)
        : Instruction(ValueTID::STORE_SSA, VoidType::voidty, OpCode::STORE),
          _target_pointer_type(target_pointer_type),
          _align(align), _source(std::move(source)), _target(std::move(target)){
        MYGL_ADD_USEE_PROPERTY(source); // [0]
        MYGL_ADD_USEE_PROPERTY(target); // [1]
    }

    StoreSSA::~StoreSSA() = default;

    void StoreSSA::set_source(owned<Value> source)
    {
        if (_source == source)
            return;
        if (source != nullptr)
            check_source_type_or_throw(source, _target_pointer_type, CURRENT_SRCLOC_F);
        Use *use = _list_as_user.front();
        if (_source != nullptr)
            _source->removeUseAsUsee(use);
        if (source != nullptr)
            source->addUseAsUsee(use);
        _source = std::move(source);
    }

    void StoreSSA::set_target(owned<Value> target)
    {
        if (_target == target)
            return;
        if (target != nullptr) {
            Type *target_vty = target->get_value_type();
            if (target_vty != _target_pointer_type &&
                !target_vty->equals(_target_pointer_type)) {
                throw TypeMismatchException {
                    target_vty,
                    osb_fmt("StoreSSA new target type should be equal to"
                    " the original target type "sv, _target_pointer_type->toString()),
                    CURRENT_SRCLOC_F
                };
            }
        }
        Use *use = _list_as_user.at(OperandOrder::TARGET);
        if (_target != nullptr)
            _target->removeUseAsUsee(use);
        if (target != nullptr)
            target->addUseAsUsee(use);
        _target = std::move(target);
    }

/*
    void StoreSSA::setSourceTarget(owned<Value> source, owned<Value> target)
    {
        if (_source == source) {
            set_target(std::move(target));
            return;
        }
        if (_target == target) {
            set_source(std::move(source));
            return;
        }
        auto iter = _list_as_user.begin();
        Use *src = iter.get();
        Use *tgt = iter.get_next_iterator().get();
        _target_pointer_type = construct_check_get_target_type(source, target, CURRENT_SRCLOC_F);
        if (_source != nullptr)
            _source->removeUseAsUsee(src);
        if (source != nullptr)
            source->addUseAsUsee(src);
        if (_target != nullptr)
            _target->removeUseAsUsee(tgt);
        if (target != nullptr)
            target->addUseAsUsee(tgt);
        _source = std::move(source);
        _target = std::move(target);
    }
*/

/* ================ [StoreSSA::override parent] ================ */
    void StoreSSA::accept(IValueVisitor &visitor) { visitor.visit(this); }

    Value *StoreSSA::operator[](size_t index)
    {
        switch (index) {
            case SOURCE: return get_source();
            case TARGET: return get_target();
            default:     return nullptr;
        }
    }
    void StoreSSA::traverseOperands(ValueAccessFunc fn)
    {
        if (fn(*get_source())) return;
        if (fn(*get_target())) return;
    }

    void StoreSSA::on_parent_plug(BasicBlock *parent)
    {
        Instruction::on_parent_plug(parent);
        _connect_status = Instruction::ConnectStatus::CONNECTED;
    }
    void StoreSSA::on_parent_finalize()
    {
        auto iter = _list_as_user.begin();
        Use *usrc = iter.get();
        Use *utgt = iter.get_next_iterator().get();
        if (_source != nullptr) {
            _source->removeUseAsUsee(usrc);
            _source.reset();
        }
        if (_target != nullptr) {
            _target->removeUseAsUsee(utgt);
            _target.reset();
        }
        _connect_status = Instruction::ConnectStatus::FINALIZED;
    }
    void StoreSSA::on_function_finalize()
    {
        _source.reset();
        _target.reset();
        _connect_status = Instruction::ConnectStatus::FINALIZED;
    }
} // namespace MYGL::IR
