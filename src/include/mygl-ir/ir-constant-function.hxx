#ifndef __MYGL_IR_FUNCTION_H__
#define __MYGL_IR_FUNCTION_H__

#include "base/mtb-object.hxx"
#include "ir-constant.hxx"
#include "ir-basic-value.hxx"
#include "ir-basicblock.hxx"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>

namespace MYGL::IR {

/** @class Function
 * @brief 函数声明或函数定义.
 *
 * Function 是典型的 Usee-User 结合体, 既可以当成 call 指令的函数指针
 * 操作数, 也可以作为基本块的容器.
 *
 * 由于函数类型不可实例化, Function 作为操作数时, 它的类型是 **函数指针
 * 类型** 而不是函数类型. 例如, 函数 `i32 main(i32 %argc, i8** %argv)`
 * 的类型是 `i32(i32, i8**)*` 而不是 `i32(i32, i8**)`.
 *
 * 上面所示的 `define/declare i32 main(i32 %0, i8** %1)` 这样的内容称为
 * 函数头, 它是函数得以成为 Value 的重要结构. 函数头记录了函数类型、参数列表
 * 等等, 还提供了一些可以快速获取返回类型等的工具方法.
 *
 * 有些函数没有函数体, 文本格式以 "declare" 修饰, 这时函数是 "函数声明";
 * 其他有函数体的函数就称为 "函数定义". 函数定义仅仅比函数声明多了一个函数体,
 * 为了实现方便, Function 类同时承担表示这两种函数语法的作用.
 *
 * 要区分一个函数是不是函数声明, 你需要 `is_declaration` 只读布尔属性.
 * 倘若你想把一个函数定义变成函数声明, 请调用 `disableBody()` 方法.
 * 倘若你想把一个函数声明变成函数定义, 请调用 `enableBody()` 方法.
 *-------------------------------------------------------------------
 *
 * 函数体是函数定义必需的结构, 本质上就是一个基本块集合. 函数调用发生以后, 执行
 * 流总会从一个基本块进入, 该基本块称为函数的**入口**(对应读写属性 `entry`).
 * 大多数函数都有一个或者几个出口基本块, 但是 Function 没有专门的属性来存储这些
 * 基本块. */
class Function final: public Definition {
public:
    /** 参数列表存储单元. */
    struct ArgUsePair {
        owned<Argument> argument; // 参数本体, 恒不为 null.
        Use            *use_arg;  // 参数的 use
    public:
        /** 由于参数本体恒不为 null, 所以不需要存储类型字段. */
        Type *get_type() const { return argument->get_value_type(); }
    }; // struct ArgUsePair

    /** 函数体存储单元, 与函数对象本体分开存储. */
    incomplete struct BodyImpl;
    incomplete struct MutableContext;

public: /* 函数体代理 -- 操作最快的一集 */
    /** @class MutableContextProxy
     * @brief 可变区域代理.  */
    struct MutableContextProxy {
        /* 实际的可变寄存器上下文, 没有所有权. */
        MutableContext *impl;

        enum FreeMutError: uint8_t {
            OK              = 0, // 没问题
            MUT_STILL_USED  = 1, // 可变寄存器仍然有指令使用
            MUT_UNALLCOATED = 2, // 可变寄存器没有被分配
            MUT_NOT_IN_FUNCTION = 3, // 该可变寄存器不属于该函数
            MUT_NULL        = 10,   // 参数为 null
            PROXY_NULL      = 11,   // 函数是 SSA 的，自己是 null
        }; // enum FreeMutError

        /** 和其他所有遍历函数一样, 返回 true 表示立即退出遍历 */
        using TraverseFunc    = bool(*)(Mutable*);

        /** 和其他所有遍历函数一样, 返回 true 表示立即退出遍历 */
        using TraverseClosure = std::function<bool(Mutable*)>;
    public:
        bool operator==(std::nullptr_t) const noexcept {
            return impl == nullptr;
        }

        /** @property function{get;} */
        Function *get_function() const noexcept;

        /** @fn count_mutable()
         * @brief 获取可变寄存器的数量. 倘若 impl == nullptr, 那就是 -1. */
        int64_t  count_mutable() const noexcept;

        /** @fn allocateMutable(Type)
         * @brief 根据申请一个类型为参数 value_type 所示的可变寄存器. */
        Mutable *allocateMutable(Type *value_type);

        /** @fn freeMutable(value)
         * @brief 尝试释放参数 value 所示的可变寄存器.
         * @return {FreeMutError} 错误代码, OK 表示成功 */
        [[nodiscard("请不要忽略返回的错误代码")]]
        FreeMutError freeMutable(Mutable *value);

        /** @fn forEachB(()=>terminates?)
         * @brief 遍历读每一个 Mutable
         * @return {int64_t} 成功被 func 调用的寄存器数量 */
        int64_t forEachB(TraverseFunc func);

        /** @fn forEach(()=>terminates?)
         * @brief 遍历读每一个 Mutable
         * @return {int64_t} 成功被 func 调用的寄存器数量 */
        int64_t forEach(TraverseClosure func);
    }; // struct MutableProxy

    struct BodyImplProxy {
        BodyImpl *impl;
    public:
        Function          *get_function()        const;
        BasicBlock::ListT *get_basicblock_list() const;
        BasicBlock        *get_entry()           const;
        bool is_ssa() const;
    }; // struct BodyImplProxy

    friend BodyImpl;
    friend BodyImplProxy;
    friend MutableContext;
    friend MutableContextProxy;
public:
    using RefT       = owned<Function>;
    using ArgListT   = std::vector<ArgUsePair>;
    using BodyRefT   = std::unique_ptr<BodyImpl>;
    using MutCtxRefT = std::unique_ptr<MutableContext>;
    friend RefT;

    /** @fn Create(type, name, parent:module, is_declaration)
     * @brief 创建一个类型为 type, 名称为 name, 父结点为 parent 的函数结点.
     * @param is_declaration 为 true 表示创建函数声明, 此时函数没有函数体; 否则
     *                   创建函数定义, 此时函数会默认创建一个 `return 0` 函数体.
     * @return nonnull 函数定义或者声明结点.
     *
     * @throws NullException
     * @throws TypeMismatchException 当 type 不是指向函数的指针类型时抛出该异常. */
    static RefT Create(PointerType *type, std::string const& name,
                       Module    *parent, bool is_declaration);

    /** @fn CreateFromFunctionType(function_type, name, parent, is_declaration)
     * @brief 创建一个类型为 function_type, 名称为 name, 父结点为 parent 的函数结点.
     * @param function_type  函数类型. 该函数会根据该类型获取对应的函数指针类型.
     * @param is_declaration 为 true 表示创建函数声明, 此时函数没有函数体; 否则
     *                   创建函数定义, 此时函数会默认创建一个 `return 0` 函数体.
     * @return nonnull 函数定义或者声明结点.
     *
     * @throws NullException
     * @throws TypeMismatchException 当 type 不是指向函数的指针类型时抛出该异常. */
    static always_inline RefT
    CreateFromFunctionType(FunctionType *function_type, std::string const& name,
                           Module *parent, bool is_declaration);
public:
    /** 根据函数对象的值类型构造一个名称为name的函数 */
    explicit Function(PointerType *value_type, std::string const& name,
                      Module      *parent,     bool     is_declaration);

    /** 析构前检查是否有 `user`. 倘若有, 报错退出. */
    ~Function() final;

public:  /* ================ [函数头] ================ */
    /** @property return_type{get;}
     *  @brief 该函数返回值的类型. */
    Type *get_return_type() const;

    /** @property function_type{get;}
     *  @brief 该函数对应的函数类型. */
    FunctionType *get_function_type() const {
        return static_cast<FunctionType*>(get_pointer_type()->get_target_type());
    }

    /** @property pointer_type{get;}
     * @brief Function 的类型是指针类型, 因此给出该属性. */
    PointerType  *get_pointer_type() const {
        return static_cast<PointerType*>(_value_type);
    }

    /** @property argument_list{get;access;} */
    ArgListT const &get_argument_list() const { return _argument_list; }
    ArgListT &      argument_list()           { return _argument_list; }

    /** @fn argumentAt[index] */
    Argument *argumentAt(size_t index) { return argument_list().at(index).argument; }

    /** @property has_argument{get;} bool */
    bool has_argument() const { return !_argument_list.empty(); }

public: /* ======== [overrides:重写父类的函数与属性] ======== */
    OVERRIDE_GET_FALSE(is_writable)
    OVERRIDE_GET_TRUE(is_function)
    OVERRIDE_GET_FALSE(is_global_variable)
    OVERRIDE_GET_FALSE(target_is_mutable)
    OVERRIDE_GET_FALSE(is_zero)
    Type *get_target_type() const final { return get_function_type(); }
    void reset() final { disableBody(); }

    void accept(IValueVisitor &visitor) final;

public:  /* ================ [函数体:开关] ================ */
    /** @property is_declaration{get;set;} bool
     *  @brief 该函数是不是声明. 倘若是函数声明, 那该函数就没有函数体. */
    bool is_declaration() const { return _body_impl == nullptr; }
    always_inline void set_is_declaration(bool is_declaration) {
        is_declaration ? disableBody(): enableBody();
    }

    /** @property is_extern{get;} final */
    bool is_extern() const final { return is_declaration(); }

    /** @fn enableBody()
     * @brief 启用函数体, 变该函数为函数定义. 调用后 `is_declaration` 属性为 true.
     * @return bool 是否实际完成了此操作. 倘若该函数已经是函数定义, 则返回 false. */
    bool enableBody();

    /** @fn disableBody()
     * @brief 禁用函数体, 变该函数为函数声明. 调用后 `is_declaration` 属性为 false.
     * @return bool 是否实际完成了此操作. 倘若该函数已经是函数声明, 则返回 false. */
    bool disableBody();
public:  /* ================ [函数体] ================ */
    BodyImplProxy get_body_proxy() const {
        return BodyImplProxy{_body_impl.get()};
    }

    /** @property body{get;access}
     *  @brief 函数体 -- 基本块列表
     *  @note 函数的入口结点不一定是第一个元素. */
    BasicBlock::ListT const& get_body() const;
    /** 函数体 -- 基本块列表
     *  @note 函数的入口结点不一定是第一个元素. */
    BasicBlock::ListT &      body();

    /** @property function_body = body
     * @warning 请注意, 该属性可能会被废弃. */
    [[deprecated("请换成 Function:: get_body() .")]]
    BasicBlock::ListT const& get_function_body() const { return get_body(); }

    /** @warning 请注意, 该属性可能会被废弃. */
    [[deprecated("请换成 Function:: body() .")]]
    BasicBlock::ListT &      function_body()           { return body(); }

    /** @property entry{get;set;}
     *  @brief 函数入口基本块. */
    BasicBlock *get_entry() const;
    /** @fn set_entry(entry)
     * @brief 设置 entry 为函数的入口基本块.
     * @throws NullException
     * @throws BasicBlock::ListT::WrongItemException 当 entry 不是 Function 的子结点时 */
    void set_entry(BasicBlock *entry);

    /** @warning 请注意, 该属性可能会被废弃. */
    [[deprecated("该属性会被替换为 get_entry(), 请及时更换.")]]
    BasicBlock *get_first_basicblock() const { return get_entry(); }

    /** @fn collectGarbage()
     * @brief 自入口基本块起按控制流和数据流扫描整个函数, 并清除没有扫描到的基本块.
     * @return {size_t} 已清除的基本块数量 */
    size_t collectGarbage();

public:  /* ================ [函数体: 二阶段, 可变变量存储区] ================ */
    /** @property is_ssa{get;} bool
     * @brief 函数是不是 SSA 的. 倘若一个函数存在可变寄存器, 那么它就不是 SSA 的。
     *        当函数不是 SSA 时，由于多条指令共用一个寄存器, 数据流关系不可追踪,
     *        函数不可优化. */
    bool is_ssa() const {
        return is_declaration()    || // 一个没有函数体的函数是 SSA 的(暴论)
               _mut_ctx == nullptr || // 一个没有可变寄存器的函数是 SSA 的
               get_mut_context().count_mutable() <= 0;
    }
    /** @property mut_context{get;}
     *  @brief 当函数不是 SSA 的, 就返回寄存器分配上下文. */
    MutableContextProxy get_mut_context() const noexcept {
        return {_mut_ctx.get()};
    }

    /** @fn enableMutContext()
     * @brief 启用可变寄存器上下文, 然后你才能申请和释放可变寄存器. */
    MutableContextProxy enableMutContext();

    enum class MutDisableError: uint8_t {
        OK            = 0,
        // 可变寄存器上下文存在已经分配的寄存器,
        // 删除上下文会导致函数体异常
        MUT_ALLOCATED = 1,
    }; // enum class DisableError
    /** @fn disableMutContext()
     * @brief 尝试禁用可变寄存器上下文, 把函数变回 SSA 的.
     * @return {DisableError} 错误代码 */
    [[nodiscard("请立即分析错误代码")]]
    MutDisableError disableMutContext();

private: /* ================ [函数头] ================ */
    ArgListT _argument_list; // 参数列表属性 -- 实例

private: /* ================ [函数体] ================ */
    BodyRefT   _body_impl;   // 函数体引用 -- 实例
    MutCtxRefT _mut_ctx;     // 可变函数体 -- 实例
}; // class Function final

} // namespace MYGL::IR


#endif
