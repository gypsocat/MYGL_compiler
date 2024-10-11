#include "mygl-ir/ir-constant-function.hxx"
#include "mygl-ir/ir-basicblock.hxx"
#include "mygl-ir/ir-instruction.hxx"
#include "mygl-ir/irbase-value-visitor.hxx"
#include "base/mtb-id-allocator.hxx"
#include "base/mtb-compatibility.hxx"
#include <cstdio>

#define _BB_LIST _body_impl->basic_blocks

namespace MYGL::IR {

using RefT = owned<Function>;

using namespace Compatibility;
using namespace std::string_literals;
using namespace std::string_view_literals;

struct Function::BodyImpl {
    Function         *parent;
    BasicBlock::ListT basic_blocks;
    BasicBlock       *entry;
public:
    void init(Function *parent) {
        this->parent = parent;

        /* basic_blocks 是内联对象, 需要增加一次引用计数来保护 */
        this->basic_blocks.setInlineInit();
        /* 插入一个 entry */
        ReturnSSA::RefT  return_ssa = ReturnSSA::CreateDefault(parent);
        BasicBlock::RefT entry      = BasicBlock::CreateWithEnding(parent, return_ssa);
        this->entry = entry;
        basic_blocks.append(entry);
    }
public: /* static */
    static BodyRefT Create(Function *parent) {
        auto ret = std::make_unique<BodyImpl>();
        ret->init(parent);
        return ret;
    }
}; // Function::BodyImpl

/* ==================== [Function::BodyImplProxy] ==================== */

Function* Function::BodyImplProxy::get_function() const {
    return impl->parent;
}
BasicBlock::ListT *Function::BodyImplProxy::get_basicblock_list() const {
    return &impl->basic_blocks;
}
BasicBlock *Function::BodyImplProxy::get_entry() const {
    return impl->entry;
}
bool Function::BodyImplProxy::is_ssa() const {
    return impl->parent->is_ssa();
}

struct Function::MutableContext {
    Function                 *parent;
    MTB::IDAllocator          reg_allocator;
    std::deque<Mutable::RefT> registers;
public:
    uint32_t count_registers() const {
        return reg_allocator.get_allocated_id_number();
    }
    Mutable *allocate(Type *type)
    {
        uint32_t id = reg_allocator.allocate();
        if (id >= registers.size()) {
            registers.push_back(Mutable::Create(type, parent, id));
            return registers.back();
        }

        Mutable::RefT& mut_slot = registers[id];
        if (mut_slot == nullptr ||
            !mut_slot->get_value_type()->equals(type)) {
            mut_slot = Mutable::Create(type, parent, id);
        }
        return mut_slot;
    }
    MutableContextProxy::FreeMutError free(Mutable *value)
    {
        using FreeMutError = MutableContextProxy::FreeMutError;
        if (value->get_parent() != parent)
            return FreeMutError::MUT_NOT_IN_FUNCTION;
        if (!reg_allocator.isAllocated(value->get_index()))
            return FreeMutError::MUT_UNALLCOATED;
        if (!value->get_list_as_usee().empty())
            return FreeMutError::MUT_STILL_USED;
        reg_allocator.free(value->get_index());
        return FreeMutError::OK;
    }
public:
    static MutCtxRefT Create(Function *parent) {
        auto ret    = std::make_unique<MutableContext>();
        ret->parent = parent;
        /* reg_allocator 是内联对象, 不可析构, 需要增加一次引用计数来保护 */
        ret->reg_allocator.setInlineInit();
        return ret;
    }
}; // Function::MutableContext


/* ==================== [Function::BodyImplProxy] ==================== */

Function *Function::
MutableContextProxy::get_function() const noexcept {
    if (impl == nullptr)
        return nullptr;
    return impl->parent;
}
int64_t Function::MutableContextProxy::count_mutable() const noexcept
{
    if (impl == nullptr)
        return -1;
    return impl->count_registers();
}
Mutable *Function::MutableContextProxy::allocateMutable(Type *value_type)
{
    if (impl == nullptr)
        return nullptr;
    return impl->allocate(value_type);
}
Function::MutableContextProxy::FreeMutError
Function::MutableContextProxy::freeMutable(Mutable *value)
{
    if (impl == nullptr)
        return FreeMutError::PROXY_NULL;
    return impl->free(value);
}
int64_t Function::MutableContextProxy::forEachB(TraverseFunc func)
{
    int ntraversed = 0;
    for (int i = 0; i < impl->registers.size(); i++) {
        if (!impl->reg_allocator.isAllocated(i))
            continue;
        if (impl->registers[i] == nullptr)
            continue;
        ntraversed++;
        if (func(impl->registers[i]))
            break;
    }
    return ntraversed;
}
int64_t Function::MutableContextProxy::forEach(TraverseClosure func)
{
    int ntraversed = 0;
    for (int i = 0; i < impl->registers.size(); i++) {
        if (!impl->reg_allocator.isAllocated(i))
            continue;
        if (impl->registers[i] == nullptr)
            continue;
        ntraversed++;
        if (func(impl->registers[i]))
            break;
    }
    return ntraversed;
}


inline namespace function_impl {
    static Function::RefT new_fn(PointerType *type, std::string const& name,
                                 Module    *parent, bool is_declaration) {
        RETURN_MTB_OWN(Function, type, name, parent, is_declaration);
    }

    static FunctionType* chk_type_throw(PointerType *vty, Module *mod, SourceLocation loc)
    {
        if (vty == nullptr) throw NullException {
            "Function.value_type"sv,
            ""s, std::move(loc)
        };
        if (mod == nullptr) throw NullException {
            "Function.module"sv,
            ""s, std::move(loc)
        };
        Type *type_target = vty->get_target_type();
        if (type_target->get_type_id() != TypeTID::FUNCTION_TYPE) {
            throw TypeMismatchException {
                vty, osb_fmt("Function value type should "
                    "be pointer to function type while the argument is `"sv,
                    type_target->toString(), "*` type"),
                std::move(loc)
            };
        }
        return static_cast<FunctionType*>(type_target);
    }
    static void chk_has_body(Function const* fn)
    {
        if (fn->get_body_proxy().impl != nullptr)
            return;

        throw NullException {
            osb_fmt("{private Function \"", fn->get_name(), "\"._body_impl}"),
            "Cannot get a function impl while the function is a declaration",
            CURRENT_SRCLOC_F
        }; // throw NullException
    }

    struct arg_vgetset_base {
        Argument *arg_unowned;
    protected:
        always_inline Use::SetResult do_set(Value *value) {
            arg_unowned->set_name(value->get_name_or_id());
            return {false};
        }
        always_inline Argument *do_get() const {
            return arg_unowned;
        }
    };
    delegate arg_vget: public arg_vgetset_base {
        Value *operator()() const { return do_get(); }
    };
    delegate arg_vset: public arg_vgetset_base {
        Use::SetResult operator()(Value *value) { return do_set(value); }
    };
} // inline namespace function_impl

/** @class Function */
/** ================ [static class Function] ================ */

RefT Function::Create(PointerType *value_type, std::string const& name,
                      Module      *parent,     bool is_declaration) {
    chk_type_throw(value_type, parent, CURRENT_SRCLOC_F);
    return new_fn(value_type, name, parent, is_declaration);
}

RefT CreateFromFunctionType(FunctionType     * fty,
                            std::string const& name,
                            Module *mod, bool is_declaration) {
    if (fty == nullptr) throw NullException {
        "Function.function_type in construct"sv,
        {}, CURRENT_SRCLOC_F
    };
    if (mod == nullptr) throw NullException {
        "Function.module in construct"sv,
        {}, CURRENT_SRCLOC_F
    };
    TypeContext *ctx = fty->get_type_context();
    PointerType *pty = ctx->getPointerType(fty);
    return new_fn(pty, name, mod, is_declaration);
}

/** ================ [public class Function] ================ */

Function::Function(PointerType *type, std::string const& name,
                   Module    *parent, bool     is_declaration)
    : Definition(ValueTID::FUNCTION, type, parent) {
    _name = name;
    auto fty = static_cast<FunctionType*>(type->get_target_type());
    auto &param = fty->get_param_list();
    uint32_t iter = 0, argnum = param.size();
    _argument_list.resize(argnum);

    /* 遍历参数表, 注册参数. 注意, Argument被Function独占所有权, 所以set_usee()动不了它 */
    for (Type *ti: param) {
        auto arg = own<Argument>(ti, "", this);
        arg->set_id(iter);
        _argument_list[iter] = {
            arg,
            addValue(arg_vget{arg},
                              arg_vset{arg})
        };
        iter++;
    }
    if (!is_declaration)
        _body_impl = BodyImpl::Create(this);
}

Function::~Function() {
    if (_body_impl == nullptr)
        return;
    for (BasicBlock *i: _body_impl->basic_blocks)
        i->on_function_finalize();
}

/* ================ [函数头] ================ */
Type *Function::get_return_type() const {
    return get_function_type()->get_return_type();
}

/* ================ [函数体:开关] ================ */

bool Function::enableBody()
{
    if (_body_impl != nullptr)
        return false;
    _body_impl = BodyImpl::Create(this);
    return true;
}
bool Function::disableBody()
{
    if (_body_impl == nullptr)
        return false;
    _body_impl.reset();
    return true;
}

/* ================ [函数体] ================ */

BasicBlock::ListT const& Function::get_body() const {
    chk_has_body(this);
    return _body_impl->basic_blocks;
}
BasicBlock::ListT& Function::body() {
    chk_has_body(this);
    return _body_impl->basic_blocks;
}
BasicBlock *Function::get_entry() const {
    chk_has_body(this);
    return _body_impl->entry;
}
void Function::set_entry(BasicBlock *entry)
{
    if (entry == get_entry())
        return;
    if (entry == nullptr) throw NullException {
        "Function::set_entry().entry"sv,
        {}, CURRENT_SRCLOC_F
    };
    if (entry->get_parent() != this) {
        throw BasicBlock::ListT::WrongItemException {
            &_body_impl->basic_blocks, entry,
            BasicBlock::ListT::WrongItemException::ErrorCode::WRONG_LIST,
            "", CURRENT_SRCLOC_F
        };
    }
    _body_impl->entry = entry;
}
size_t Function::collectGarbage() {
    return 0;
}

/* ================ [函数体: 二阶段, 可变变量存储区] ================ */

Function::MutableContextProxy
Function::enableMutContext()
{
    _mut_ctx = MutableContext::Create(this);
    return {_mut_ctx.get()};
}
Function::MutDisableError
Function::disableMutContext()
{
    if (_mut_ctx->count_registers() != 0)
        return MutDisableError::MUT_ALLOCATED;
    _mut_ctx.reset();
    return MutDisableError::OK;
}

/* ================ [Function:: overrides parent] ================ */
void Function::accept(IValueVisitor &visitor) {
    visitor.visit(this);
}

} // namespace MYGL::IR
