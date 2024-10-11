#include "base/mtb-object.hxx"
#include "mygl-ir/ir-basic-value.hxx"
#include "mygl-ir/ir-basicblock.hxx"
#include "mygl-ir/ir-constant.hxx"
#include "mygl-ir/ir-instruction.hxx"
#include "mygl-ir/ir-module.hxx"
#include "mygl-ir/irbase-type.hxx"
#include "mygl-ir/irbase-use-def.hxx"
#include "mygl-ir/utils/ir-util-writer.hxx"
#include <cstddef>
#include <format>
#include <string>
#include <string_view>

using OpCode = MYGL::IR::Instruction::OpCode;
using std::string_view;
using twine = string_view;
using std::string;

#define do_write(fmt, ...) stream() << std::format(fmt __VA_OPT__(,) __VA_ARGS__)

template<typename IsT, typename ObjT>
bool is(ObjT *obj) { return dynamic_cast<IsT*>(obj) != nullptr; }

template<typename IsT, typename ObjT>
bool is(const ObjT *obj) { return dynamic_cast<const IsT*>(obj) != nullptr; }

namespace MYGL::IRUtil {

inline namespace writer_impl {
    using namespace std::string_view_literals;
    using namespace std::string_literals;

    static std::string_view declaration_map[2] = {
        "define", "declare"
    };

    static string_view get_id_prefix(const Value *value) {
        return is<Constant>(value)? "@": "%";
    }
}

/** @class Writer */

Writer::Writer(owned<Module> module, bool llvm_compatible)
    : _module(std::move(module)),
      _indent(0), _llvm_compatible(llvm_compatible),
      _indent_space("  ") {}
Writer::~Writer() = default;

void Writer::visit(Module *module)
{
    do_write("; module {}\n", module->get_name());

    for (auto &i: module->global_variables())
        i.second->accept(*this);

    for (auto &i: module->functions())
        i.second->accept(*this);
}

/** @brief 全局变量
 * - 语法:
 *   - `@id = external dso_local [global|constant] <type>, [align <align>]`
 *   - `@id = dso_local [global|constant] <type> <value>, [align <align>]` */
void Writer::visit(GlobalVariable *value)
{
    static string_view global_mut_map[2] = {
        "constant", "global"
    };
    static string_view decl_map[2] = {
        "", "external"
    };

    string name = value->get_name_or_id();
    twine  decl = decl_map[value->is_declaration()];
    twine  mut_name  = global_mut_map[value->target_is_mutable()];
    string type_name = value->get_target_type()->toString();
    size_t align     = value->get_align();

    do_write("@{} = {} dso_local {} {}", name, decl, mut_name, type_name);
    Constant *const_target = value->get_target();
    _write_operand(value);
}

void Writer::visit(IntConst *value) {
    IntType *ity = static_cast<IntType*>(value->get_value_type());
    if (ity->get_binary_bits() == 1) {
        do_write("{}", value->is_zero() ? "false": "true");
    } else {
        do_write("{}", value->get_int_value());
    }
}
void Writer::visit(FloatConst *value) {
    do_write("{}", value->get_value());
}
void Writer::visit(ZeroConst *value) {
    (void)value;
    do_write("0");
}
void Writer::visit(UndefinedConst *value) {
    (void)value;
}
/** @brief 数组表达式
 * - 语法: [<type> <value1>, <type> <value2>, ...]
 * (方括号不是可选，是真的方括号) */
void Writer::visit(Array *value)
{
    Type  *elem_type = value->get_element_type();
    string elem_typename = elem_type->toString();

    do_write("[");

    for (size_t cnt = 1;
         Value *i: value->element_list()) {
        do_write("{}", elem_typename);
        _write_operand(i);
        if (cnt != value->get_element_list().size())
            do_write(",");
    }
    do_write("]");
}

void Writer::visit(Function *value)
{
    /* 函数头:
     * - 语法:
     *   - `declare dso_local <type> @<name>([<type1>, <type2>, ...])`
     *   - `define  dso_local <type> @<name>([<type1> %<arg1>, <type2> %<arg2>, ...]) */
    string_view decldef = declaration_map[value->is_declaration()];
    string fn_name = value->get_name_or_id();
    string return_typestr = value->get_return_type()->toString();
    do_write("{} dso_local {} @{}(",
             decldef, return_typestr, fn_name);
    for (size_t cnt = 1;
         auto &pi : value->get_argument_list()) {
        Argument *i = pi.argument;
        do_write("{}", i->get_value_type()->toString());
        if (!value->is_declaration())
            do_write("%{}", i->get_name_or_id());
        if (cnt != value->get_argument_list().size())
            do_write(", ");
        cnt++;
    }
    do_write(")");

    /* 函数体 */
    if (value->is_declaration()) return;
    do_write("{}", " {\n");

    /* 函数体的入口基本块, 要排在第一个写入 */
    BasicBlock *entry = value->get_entry();
    do_write("{}:", entry->get_name_or_id());
    entry->accept(*this);
    if (entry != value->body().back())
        stream() << "\n\n";

    for (BasicBlock *i: value->body()) {
        /* 跳过入口基本块 */
        if (i == entry)
            continue;

        /** 写入BasicBlock的ID */
        do_write("{}:", i->get_name_or_id());
        i->accept(*this);
        if (i != value->body().back())
            stream() << "\n\n";
    }
    do_write("\n}}\n");
}
void Writer::visit(BasicBlock *value)
{
    add_indent();
    for (Instruction *i: value->instruction_list()) {
        wrap_indent();
        i->accept(*this);
    }
    dec_indent();
}
void Writer::visit(Argument *value) {
    do_write("{}", value->get_name_or_id());
}
/* 以下均为指令结点的visit方法。语法见指令结点自己的注释。 */
void Writer::visit(PhiSSA *value)
{
    string type_string = value->get_value_type()->toString();
    do_write("%{} = phi {} ", value->get_name_or_id(), type_string);

    /* 被Phi引用的值列表 */
    for (size_t cnt = 1;
         auto &i : value->get_operands()) {
        do_write("[ ");
        _write_operand(i.second.value);
        if (_llvm_compatible)
            do_write(", %{} ]", i.first->get_name_or_id());
        else
            do_write(", label %{} ]", i.first->get_name_or_id());
        if (cnt != value->get_operands().size())
            do_write(", ");
        cnt++;
    }
}
void Writer::visit(LoadSSA *value)
{
    Value *operand = value->get_operand();
    /* LoadSSA本身的类型 */
    string type_string    = value
                          ->get_value_type()
                          ->toString();
    /* 操作数的类型 */
    string ptr_typestring = operand
                          ->get_value_type()
                          ->toString();

    do_write("%{} = load {}, {} {}{}, align {}",
             value->get_name_or_id(), type_string, ptr_typestring,
             get_id_prefix(operand),
             operand->get_name_or_id(),
             value->get_align());
}
void Writer::visit(CastSSA *value)
{
    Value *operand = value->get_operand();
    string_view opcode = value->get_opcode().getString();
    string src_type = operand->get_value_type()->toString();
    string dst_type = value->get_value_type()->toString();
    do_write("%{} = {} {} ",
             value->get_name_or_id(), opcode, src_type);
    _write_operand(operand);
    do_write("to {}", dst_type);
}
void Writer::visit(UnaryOperationSSA *value)
{
    Value *operand = value->get_operand();
    string_view opcode = value->get_opcode().getString();
    string type = operand->get_value_type()->toString();
    do_write("%{} = {} {} ", value->get_name_or_id(), opcode, type);
    _write_operand(operand);
}
void Writer::visit(AllocaSSA *value)
{
    do_write("%{} = alloca {}, align {}",
             value->get_name_or_id(),
             value->get_element_type()->toString(),
             value->get_align());
}
void Writer::visit(BinarySSA *value)
{
    Value *lhs = value->get_lhs();
    Value *rhs = value->get_rhs();
    string_view opcode = value->get_opcode().getString();
    string type = value->get_value_type()->toString();
    string sign_flag(BinarySSA::SignFlagGetString(
                     value->get_sign_flag()));
    if (!sign_flag.empty())
        sign_flag.append(" ");

    do_write("%{} = {} {}{} ",
             value->get_name_or_id(), opcode, sign_flag, type);

    _write_operand(lhs);
    do_write(", ");
    _write_operand(rhs);
}
void Writer::visit(UnreachableSSA *value)
{
    do_write("unreachable");
}
void Writer::visit(JumpSSA *value)
{
    BasicBlock *target = value->get_target();
    do_write("br label %{}", target->get_name_or_id());
}
void Writer::visit(BranchSSA *value)
{
    BasicBlock *true_target  = value->get_if_true();
    BasicBlock *false_target = value->get_if_false();
    Value      *condition    = value->get_condition();
    do_write("br i1 ");
    _write_operand(condition);
    do_write(", label %{}, label %{}",
             true_target->get_name_or_id(),
             false_target->get_name_or_id());
}
void Writer::visit(SwitchSSA *value)
{
    Value *condition      = value->get_condition();
    string condition_type = condition->get_value_type()->toString();
    do_write("switch {} ", condition_type);
    _write_operand(condition);

    do_write(", label %{} [", value->get_default_target()->get_name_or_id());
    add_indent();
    /* case列表。switch指令应该是唯一一个边写边换行的指令. */
    for (auto &i : value->get_cases()) {
        wrap_indent();
        do_write("i64 {}, label %{}", i.first, i.second.target->get_name_or_id());
    }
    dec_indent();
    do_write("]");
}
void Writer::visit(CallSSA *value)
{
    Function *fn = value->get_callee();
    string name = value->get_name_or_id();
    string ret_type = value->get_value_type()->toString();
    do_write("%{} = call {} @{}(",
             name, ret_type, fn->get_name_or_id());

    for (size_t cnt = 1;
         auto &[i, t, u]: value->get_arguments()) {
        do_write("{} noundef ", i->get_value_type()->toString());
        _write_operand(i);
        if (cnt != value->get_arguments().size())
            do_write(", ");
        cnt++;
    }
    do_write(")");
}
void Writer::visit(ReturnSSA *value)
{
    do_write("ret {} ", value->get_return_type()->toString());
    // 由于Undefined不打印，所以不用特殊处理"ret void"的情况
    _write_operand(value->get_result());
}
void Writer::visit(GetElemPtrSSA *value)
{
    Value *collection = value->get_collection();
    string name  = value->get_name_or_id();
    string collection_type = value->get_collection_type()->toString();
    string value_type   = value->get_value_type()->toString();
    string operand_type = collection->get_value_type()->toString();

    do_write("%{} = getelementptr inbounds {}, {} ",
             name, value_type, operand_type);
    _write_operand(collection);
    for (size_t cnt = 0;
         Value *i: value->get_indexes()) {
        do_write(", {} ", i->get_value_type()->toString());
        _write_operand(i);
    }
}
void Writer::visit(StoreSSA *value)
{
    Value *source = value->get_source();
    Value *target = value->get_target();
    Type *src_type = source->get_value_type();
    Type *dst_type = target->get_value_type();
    do_write("store {} ", src_type->toString());
    _write_operand(source);
    do_write(", {} ", dst_type->toString());
    _write_operand(target);
    do_write(", align {}", value->get_align());
}
void Writer::visit(CompareSSA *value)
{
    Value *lhs = value->get_lhs();
    Value *rhs = value->get_rhs();
    /** TODO: 重写该方法. */
}
void Writer::visit(MoveInst *value) {
    /** TODO: 重写该方法. */
}
/** private class Writer */
void Writer::_write_operand(Value *operand)
{
    if (operand->uniquely_referenced())
        stream() << std::format("{}{}", get_id_prefix(operand), operand->get_name_or_id());
    else
        operand->accept(*this);
}

/** end class Writer */

} // namespace MYGL::IRUtil