#include "base/mtb-exception.hxx"
#include "mygl-ir/ir-basicblock.hxx"
#include "mygl-ir/ir-instruction.hxx"
#include "mygl-ir/irbase-value-visitor.hxx"
#include "irbase-capatibility.private.hxx"

namespace MYGL::IR {
    inline namespace call_ssa_impl {
        static Type *get_return_type_or_throw(Function *callee)
        {
            if (callee == nullptr) {
                throw NullException {
                    "CallSSA.callee",
                    "", CURRENT_SRCLOC_F
                };
            }
            return callee->get_return_type();
        }

        struct arg_vsetter {
            CallSSA *caller;
            int   arg_index;
        public:
            Use::SetResult operator()(Value *usee)
            {
                CallSSA::ArgUseTuple &arguse_ref = caller->arguments()[arg_index];
                owned<Value> &usee_ref = arguse_ref.arg;
                Use           *arg_use = arguse_ref.arg_use;
                if (usee_ref == usee)
                    return Use::SetResult{false};
                if (usee_ref != nullptr)
                    usee_ref->removeUseAsUsee(arg_use);
                if (usee != nullptr)
                    usee_ref->addUseAsUsee(arg_use);
                return Use::SetResult{false};
            }
        }; // struct arg_vsetter

        struct arg_vgetter {
            CallSSA *caller;
            int   arg_index;
        public:
            Value *operator()() const {
                return caller->get_arguments().at(arg_index).arg;
            }
        }; // struct arg_vgetter
    } // inline namespace call_ssa_impl

    CallSSA::CallSSA(owned<Function> callee, ValueArrayT const& arguments)
        : Instruction(ValueTID::CALL_SSA,
                      get_return_type_or_throw(callee),
                      OpCode::CALL),
          _callee(std::move(callee)),
          _arguments(0) {
        using namespace std::string_view_literals;
        using namespace std::string_literals;
        using namespace capatibility;

        /* 检查参数数量是否正常 */
        FunctionType *function_type = _callee->get_function_type();
        size_t     callee_arg_nmemb = function_type->get_param_nmemb();
        if (arguments.size() != _callee->get_argument_list().size()) {    
            throw MTB::Exception {
                ErrorLevel::CRITICAL,
                osb_fmt("Encounted unmanaged exception"
                    " in CallSSA constructing: Function type `"sv,
                    function_type->toString(),
                    "` requires "sv, callee_arg_nmemb,
                    " arguments, but you only passed "sv, arguments.size(),
                    " arguments"sv),
                CURRENT_SRCLOC_F
            };
        }

        MYGL_ADD_TYPED_USEE_PROPERTY_FINAL(Function, FUNCTION, callee);

        /* 检查并初始化参数列表. 若发现参数不匹配，则丢异常 */
        _arguments.resize(callee_arg_nmemb);
        int  count = 0;
        auto from_iter = arguments.begin();
        auto to_iter   = _arguments.begin();
        while (from_iter != arguments.end()) {
            Value *arg = *from_iter;
            if (arg == nullptr) {
                throw NullException {
                    osb_fmt("CallSSA.arguments["sv, count, "]"sv),
                    "Call expression argument cannot be null while constructing"s,
                    CURRENT_SRCLOC_F
                }; // throw NullException
            }
            to_iter->arg      = arg;
            to_iter->arg_type = arg->get_value_type();
            to_iter->arg_use  = addValue(arg_vgetter{this, count},
                                        arg_vsetter{this, count});
            to_iter++, from_iter++, count++;
        }
    }

    void CallSSA::set_callee(owned<Function> callee)
    {
        if (callee == _callee)
            return;
        Use *callee_use = _list_as_user.at(0);
        if (_callee != nullptr)
            _callee->removeUseAsUsee(callee_use);
        if (callee != nullptr)
            callee->addUseAsUsee(callee_use);
        _callee = std::move(callee);
    }

    void CallSSA::setArgument(size_t index, owned<Value> argument)
    {
        using namespace capatibility;
        ArgUseTuple &tuple = _arguments.at(index);
        owned<Value> &primary_arg = tuple.arg;
        if (tuple.arg == argument)
            return;
        /* argument为空, 表示清除参数 */
        if (argument == nullptr) {
            if (primary_arg != nullptr)
                primary_arg->removeUseAsUsee(tuple.arg_use);
            return;
        }

        MTB_UNLIKELY_IF (Type *new_argtype = argument->get_value_type();
            tuple.arg_type->equals(new_argtype) == false) {
            throw TypeMismatchException {
                new_argtype,
                osb_fmt("new argument type [",
                    new_argtype->toString(),
                    "] should be equal to primary argument type ",
                    tuple.arg_type->toString()),
                CURRENT_SRCLOC_F
            };
        }

        if (primary_arg != nullptr)
            primary_arg->removeUseAsUsee(tuple.arg_use);
        argument->addUseAsUsee(tuple.arg_use);
        primary_arg = std::move(argument);
    }

/* ================ [CallSSA::override parent] ================ */
    void CallSSA::accept(IValueVisitor &visitor)
    {
        visitor.visit(this);
    }
    Value *CallSSA::operator[](size_t index)
    {
        if (index == 0) return _callee;
        return _arguments.at(index - 1).arg;
    }
    void CallSSA::traverseOperands(ValueAccessFunc fn)
    {
        if (fn(*_callee)) return;
        for (ArgUseTuple &arg: _arguments) {
            if (fn(*arg.arg))
                return;
        }
    }
    size_t CallSSA::get_operand_nmemb() const
    {
        return 1 + _arguments.size();
    }
/* ================ [CallSSA::override signal] ================ */
    void CallSSA::on_parent_plug(BasicBlock *parent) {
        Instruction::on_parent_plug(parent);
        _connect_status = Instruction::ConnectStatus::CONNECTED;
    }
    void CallSSA::on_parent_finalize()
    {
        set_callee(nullptr);
        for (ArgUseTuple &arg: _arguments) {
            if (arg.arg == nullptr)
                continue;
            arg.arg->removeUseAsUsee(arg.arg_use);
            arg.arg.reset();
        }
        _connect_status = Instruction::ConnectStatus::FINALIZED;
    }
    void CallSSA::on_function_finalize()
    {
        set_callee(nullptr);
        for (ArgUseTuple &arg: _arguments)
            arg.arg.reset();
        _connect_status = Instruction::ConnectStatus::FINALIZED;
    }
} // namespace MYGL::IR
