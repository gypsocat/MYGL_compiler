#include "base/mtb-exception.hxx"
#include "mygl-ir/ir-basicblock.hxx"
#include "mygl-ir/ir-instruction.hxx"
#include "mygl-ir/irbase-value-visitor.hxx"
#include "irbase-capatibility.private.hxx"

namespace MYGL::IR {
/* ================ [class UnarySSA] ================ */
    namespace unary_ssa_impl {
    } // inline namespace unary_ssa_impl
/* ============= [public class UnarySSA] ============ */
    bool UnarySSA::set_operand(owned<Value> operand)
    {
        Use *operand_use = _list_as_user.at(0);
        if (_operand == operand)
            return true;
        if (checkOperand(operand) == false)
            return false;

        if (_operand != nullptr)
            _operand->removeUseAsUsee(operand_use);
        if (operand != nullptr)
            operand->addUseAsUsee(operand_use);
        _operand = std::move(operand);
        return true;
    }

    UnarySSA::UnarySSA(ValueTID type_id, Type *    value_type,
                       OpCode   opcode,  owned<Value> operand)
        : Instruction(type_id, value_type, opcode),
          _operand(std::move(operand)) {
        /* Static Usee [operand] at [0] */
        MYGL_ADD_TYPED_USEE_PROPERTY(Value, operand);
    }
/* ============== [end class UnarySSA] ============== */

/* ================ [class LoadSSA] ================= */
    namespace load_ssa_impl {
        using namespace capatibility;

        static Type *pointer_operand_get_value_type(Value *operand)
        {
            if (operand == nullptr) {
                throw NullException {
                    "LoadSSA.operand in construct",
                    "LoadSSA cannot take null as operand while constructing",
                    CURRENT_SRCLOC_F
                };
            }
            Type *operand_type = operand->get_value_type();
            if (!operand_type->is_pointer_type()) {
                throw TypeMismatchException {
                    operand_type,
                    "LoadSSA requires pointer type as operand type",
                    CURRENT_SRCLOC_F
                };
            }
            PointerType *operand_pty = static_cast<PointerType*>(operand_type);
            Type *elem_type = operand_pty->get_target_type();
            if (elem_type->is_void_type()) {
                throw TypeMismatchException {
                    operand_type,
                    osb_fmt("LoadSSA cannot load anything"
                        " from a `void*` pointer operand ", operand->get_name_or_id()),
                    CURRENT_SRCLOC_F
                };
            }
            return elem_type;
        }
    } // namespace load_ssa_impl
/* ============= [public class LoadSSA] ============= */
    LoadSSA::LoadSSA(owned<Value> pointer_operand)
        : UnarySSA(ValueTID::LOAD_SSA,
                   load_ssa_impl::pointer_operand_get_value_type(pointer_operand),
                   OpCode::LOAD, pointer_operand) {
        _source_pointer_type = static_cast<PointerType*>(pointer_operand->get_value_type());
        _align = get_value_type()->get_instance_align();
    }
    LoadSSA::LoadSSA(owned<Value> pointer_operand, size_t align)
        : UnarySSA(ValueTID::LOAD_SSA,
                   load_ssa_impl::pointer_operand_get_value_type(pointer_operand),
                   OpCode::LOAD, pointer_operand),
          _align(fill_to_power_of_two(align)) {
        _source_pointer_type = static_cast<PointerType*>(pointer_operand->get_value_type());
    }

    void LoadSSA::set_align(size_t align) {
        _align = fill_to_power_of_two(align);
    }

    bool LoadSSA::checkOperand(Value *operand)
    {
        /* operand 为 null, 则表示清空操作 */
        if (operand == nullptr)
            return true;
        Type *new_operand_type = operand->get_value_type();
        if (operand->get_value_type()->equals(_source_pointer_type))
            return true;
        throw TypeMismatchException {
            new_operand_type,
            "new operand type should equal to"
                " `source_pointer_type` property",
            CURRENT_SRCLOC_F
        };
    }

    void LoadSSA::accept(IValueVisitor &visitor) {
        visitor.visit(this);
    }
/* =========== [LoadSSA::override signal] =========== */
    void LoadSSA::on_parent_plug(BasicBlock *parent) {
        Instruction::on_parent_plug(parent);
        _connect_status = Instruction::ConnectStatus::CONNECTED;
    }
    void LoadSSA::on_parent_finalize()
    {
        set_operand(nullptr);
        _connect_status = ConnectStatus::FINALIZED;
    }
    void LoadSSA::on_function_finalize() {
        on_parent_finalize();
    }
/* ============== [end class LoadSSA] =============== */

} // MYGL::IR
