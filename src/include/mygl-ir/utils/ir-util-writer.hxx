#ifndef __MYGL_IR_UTIL_WRITER_H__
#define __MYGL_IR_UTIL_WRITER_H__

#include "base/mtb-getter-setter.hxx"
#include "base/mtb-object.hxx"
#include "base/mtb-exception.hxx"
#include "mygl-ir/ir-instruction.hxx"
#include "mygl-ir/ir-basic-value.hxx"
#include "mygl-ir/ir-constant.hxx"
#include "mygl-ir/ir-module.hxx"
#include "mygl-ir/irbase-value-visitor.hxx"

namespace MYGL::IRUtil {
using namespace IR;
using namespace IRBase;
using namespace MTB;

class Writer: public MTB::Object,
              public IRBase::IValueVisitor {
public:
    explicit Writer(owned<Module> module, bool llvm_compatible = false);
    ~Writer() override;

    /** @property module{get;set;} owned */
    Module *get_module() const { return _module; }
    void set_module(owned<Module> module) {
        _module = std::move(module);
    }

    /** @property stream{get;set;} */
    std ::ostream &get_stream() const { return *_stream; }
    void set_stream(std ::ostream &stream) { _stream = &stream; }
    std::ostream &stream() { return *_stream; }

    /** @property indent{get;set;}
     *  @brief 缩进系数, 表示要缩进多少个tab. */
    size_t get_indent() const       { return  _indent; }
    void   set_indent(size_t value) { _indent = value; }

    /** @property llvm_compatible{get;set;} bool */
    bool llvm_compatible() const { return _llvm_compatible; }
    void set_llvm_compatible(bool llvm_compatible) {
        _llvm_compatible = llvm_compatible;
    }

    /** @property shows_alias_target{get;set;} bool */
    bool shows_alias_target() const { return _shows_alias_target; }
    void set_shows_alias_target(bool shows_alias_target) {
        _shows_alias_target = shows_alias_target;
    }

    void add_indent() { _indent++; }
    void dec_indent() { _indent--; }
    void indent() {
        for (int i = 0; i < get_indent(); i++)
            stream() << _indent_space;
    }
    void wrap_indent() {
        stream() << std::endl; indent();
    }

    void write(std::ostream &stream)
    {
        set_stream(stream);
        if (_module == nullptr)
            throw NullException("module");
        visit(get_module());
        /* 同步缓冲区 */
        stream.flush();
    }

    void visit(IntConst   *value) override;
    void visit(FloatConst *value) override;
    void visit(ZeroConst  *value) override;
    void visit(UndefinedConst *value) override;
    void visit(Array      *value) override;
    void visit(Function   *value) override;
    void visit(GlobalVariable *value) override;
    void visit(BasicBlock *value) override;
    void visit(Argument   *value) override;
    /* 指令区 */
    void visit(PhiSSA     *value) override;
    void visit(LoadSSA    *value) override;
    void visit(CastSSA    *value) override;
    void visit(UnaryOperationSSA *value) override;
    void visit(MoveInst   *value) override;
    void visit(AllocaSSA  *value) override;
    void visit(BinarySSA  *value) override;
    void visit(UnreachableSSA    *value) override;
    void visit(JumpSSA    *value) override;
    void visit(BranchSSA  *value) override;
    void visit(SwitchSSA  *value) override;
    void visit(CallSSA    *value) override;
    void visit(ReturnSSA  *value) override;
    void visit(GetElemPtrSSA *value) override;
    void visit(StoreSSA   *value) override;
    void visit(CompareSSA *value) override;
    /* Module */
    void visit(Module    *module) override;
private:
    owned<Module>  _module;
    std::ostream  *_stream;
    size_t         _indent;
    std::string    _indent_space;
    bool           _llvm_compatible;
    bool           _shows_alias_target;

    /** @fn _write_operand(value)
     * @brief 根据引用唯一性等性质把 User 的操作数格式化为字符串. */
    void _write_operand(Value *operand);

}; // class Writer

}; // namespace MYGL::IRUtil

#endif