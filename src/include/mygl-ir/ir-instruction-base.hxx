#pragma once
#include <cstdint>
#ifndef __MYGL_IR_INSTRUCTION_H__
#define __MYGL_IR_INSTRUCTION_H__

#include "base/mtb-object.hxx"
#include "base/mtb-getter-setter.hxx"
#include "base/mtb-classed-enum.hxx"
#include "base/mtb-exception.hxx"
#include "base/mtb-reflist.hxx"
#include "irbase-use-def.hxx"
#include "irbase-type.hxx"
#include <set>
#include <string>
#include <type_traits>
#include <functional>
#include <string_view>

namespace MYGL::IR {

using namespace MTB;
using namespace IRBase;

class Module;
class Function;
class BasicBlock;
interface IBasicBlockTerminator;

/** @class Instruction
 * @brief 指令结点, 同时表示指令执行的结果. 指令的类型就是结果的类型.
 *
 * 本来打算把Instruction与BasicBlock的特点抽象出来做成一个单独的列表类, 但是由于C++
 * 的模板类型系统让我脑壳疼，所以干脆不做了。现在我会使用自己写的一个链表类来管理Instruction
 * 与BasicBlock。尽管如此, 我仍然会为Instruction添上prev与next两个结点, 这样Instruction
 * 之间就方便查找了。BasicBlock在插入与删除指令时需要同时维护prev与next.
 *
 * - 注释编写要求: 作者至少需要为每个指令编写下列章节. 倘若读者没有发现这些章节, 那么需要
 *   向作者提出修改要求.
 *   - 指令语法: 非抽象指令类必需. 对于操作码多样的指令, 需要使用子列表case来展示这些语法.
 *   - 使用关系: 要指明每个指令的哪些成员Value要添加进Use列表.
 * - 读者指南: Instruction类几乎都非常大, 具体的阅读方法我会在图中注明(TODO: 修改成员图,
 *   添加与设计思路、阅读方法有关的注释). 读者可以按照下述优先级简要浏览这些属性与方法:
 *   - 作为操作数的属性: 例如SwitchSSA的case列表, 是加入Use列表或者持有引用的成员.
 *   - 重写的operator[]方法、traverseAccess方法: 这些方法是对所有操作数的汇总，你可以通
 *     过这些方法快速浏览所有的操作数。
 *   - 布尔类型的只读属性、读写属性：表示一个指令类都有哪些性质。
 *   - 带有OWNED标识的属性: 表示对这个属性持有所有权，且在get/set时需要特别关注所有权。 */
imcomplete class Instruction: public User {
public: /* TypeDefs and Metadata */
    struct ListAction;
    using RefT  = owned<Instruction>;
    using OperandListT  = ValueListT;
    using OperandArrayT = ValueArrayT;
    using ListT  = RefList<Instruction, ListAction>;
    using ProxyT = RefListProxy<Instruction, ListAction>;

    using ReadFunc   = std::function<bool(Instruction const&)>;
    using AccessFunc = std::function<bool(Instruction&)>;
    using ValueAccessFunc = std::function<bool(Value&)>;
    using ValueReadFunc   = std::function<bool(Value const&)>;

    friend RefT;
    friend ListT;
    friend ProxyT;
    friend struct ListAction;
    friend class BasicBlock;

    using BasicBlockUSetT = std::set<BasicBlock*>;

    /** @fn TryCastToTerminator(self) */
    static mtb_noinline IBasicBlockTerminator*
    TryCastToTerminator(Instruction *self);

    static always_inline IBasicBlockTerminator const*
    TryCastToTerminator(Instruction const* self) {
        return TryCastToTerminator(const_cast<Instruction*>(self));
    }
public:
    /** @enum OpCode
     * @brief 指令的操作码。实际使用中你不需要管OpCode这个结构体的运算符重载是怎么样
     *        实现的，因为OpCode它就是一个普通枚举、普通整数.
     *
     * 该类是enum的另一种表现形式, 用于enum自带方法较多时使用. */
    struct OpCode {
    public:
        enum self_t: uint16_t {
            NONE = 0x0,
            PHI,              // Phi指令
            JUMP, BR, SWITCH, // 跳转类
            SELECT,           // 条件赋值类
            MOVE, ALLOCA, LOAD, STORE,
            ITOF, UTOF, FTOI, // 整型-浮点转换, UTOF为uitofp
            ZEXT, SEXT, BITCAST, TRUNC,   // 重解释转换
            FPEXT, FPTRUNC,     // 浮点间转换
            INEG, FNEG, NOT,
            /* BinarySSA */
            ADD,  FADD, SUB,  FSUB, MUL,  FMUL,
            SDIV, UDIV, FDIV, UREM, SREM, FREM, // 除法、取余
            AND,  OR,   XOR,  SHL,  LSHR, ASHR,
            /* end BinarySSA */
            CALL, RET,
            GET_ELEMENT_PTR, EXTRACT_ELEMENT, INSERT_ELEMENT,
            MEMSET, MEMMOVE,
            ICMP, FCMP,
            UNREACHABLE,      // 不可达指令
            OPCODE_RESERVED_FOR_COUNTING
        }; // enum self_t: uint16

        MTB_MAKE_CLASSED_ENUM(OpCode)
        std::string_view getString() const;

        /** @brief 属性区: 表示该OpCode应该有哪些特征
         *
         * 调用方式:
         * - 作为`OpCode`结构体时直接调用: 例如`opcode.is_add()`
         * - 作为具体的`self_t`枚举常量时, 先用`OpCode`包装再调用:
         *   - 例如`OpCode(OpCode::FADD).is_add()` */
        constexpr bool is_swappable() const noexcept;
        constexpr bool is_add() const noexcept { return self == ADD | self == FADD; }
        constexpr bool is_sub() const noexcept { return self == SUB | self == FSUB; }
        constexpr bool is_mul() const noexcept { return self == MUL | self == FMUL; }
        constexpr bool is_div() const noexcept { return self == SDIV || self == UDIV || self == FDIV; }

        /** 操作数和返回值是否都是整数 */
        constexpr bool is_integer_op() const noexcept;

        /** 操作数是否都是整数(不计返回值) */
        constexpr bool operand_requires_int() const noexcept;

        /** 操作数和返回值是否都是浮点 */
        constexpr bool is_float_op() const noexcept;

        /** 操作数是否都是浮点(不计返回值) */
        constexpr bool operand_requires_float() const noexcept;

        constexpr bool is_cast_op()  const noexcept;
        constexpr bool is_logical_op() const noexcept;
        constexpr bool is_binary_op() const noexcept {
            return self >= ADD && self <= ASHR;
        }
        constexpr self_t get()  const noexcept { return self; }
        constexpr size_t hash() const noexcept { return self; }

        self_t &get() noexcept { return self; }
    }; // enum OpCode

    class OpCodeMismatchException;

    /** @enum ConnectStatus
     * @brief 指令结点的连接状态, 同一条指令在不同连接状态下的语义不同
     *
     * `ConnectStatus` 包含下述几个状态:
     * - `CONNECTED`: 连接在 BasicBlock 上, 单个指令表达完整语义.
     * - `REPARENT`:  连接在其他指令(例如 MoveInst)上. `REPARENT`
     *                状态下的指令组很难确定引用关系、不可优化、极容易
     *                出错, 除非 IR 处于寄存器分配-转译到汇编的阶段
     *                , 否则不准有处于 `REPARENT` 状态的指令.
     * - `DISCONNECTED`: 从 BasicBlock 上断开, 但是还没析构. 一般
     *                出现在指令需要在两个基本块之间转移的情况. 这时
     *                倘若触发析构, 则报错退出.
     * - `FINALIZED`: 正在析构或析构完成. 处于该状态的指令不会检查引用
     *                关系, 适合函数需要清理时直接释放目标的快速析构路
     *                径. */
    enum class ConnectStatus: uint8_t {
        CONNECTED    = 0x0, ///< 连接在BasicBlock上, 表达完整语义
        REPARENT     = 0x1, ///< 连接在其他指令上, 与父指令一同表达语义
        DISCONNECTED = 0x2, ///< 游离状态, 不连接在任何其他Value上, 但是也不析构
        FINALIZED    = 0x3, ///< 析构状态, 此时无法调用任何方法.
    }; // enum class ConnectStatus

public: /* ======== [Instruction as Operand List] ======== */
    /** @property operand_nmemb
     * @brief 把Instruction当成操作数列表看, 会有多少个操作数 */
    abstract size_t get_operand_nmemb() const = 0;

    /** @fn indexAccess() abstract
     * @brief 把Instruction当成操作数列表看, 这个是取索引表达式 */
    abstract Value *operator[](size_t index) = 0;

    /** @fn traverseOperands{(Value)->bool} abstract
     * @brief 遍历所有的操作数 */
    abstract void traverseOperands(ValueAccessFunc fn) {(void)fn;}

    /** @fn traverseOperands{(const Value)->bool} abstract
     * @brief 遍历读取所有的操作数 */
    abstract void traverseOperands(ValueReadFunc fn) const {(void)fn;}

public: /* ======= [Instruction as User] ======== */
    /** @property opcode{get;construct;} */
    OpCode get_opcode() const { return _opcode; }

    /** @property virtual inst_result{get;access;}
     * @brief 指令执行的结果. 在有执行结果的`SSA`指令中, 指令执行的结果
     *        就是自己; 在非`SSA`指令中, 指令执行的结果由专门的操作数
     *        表示. */
    virtual const Value *get_inst_result() const { return this; }
    virtual Value *inst_result() { return this; }

    /** @property is_ssa{get;} bool virtual
     *  @brief 表示该指令是否是`SSA`的. */
    DEFINE_DEFAULT_TRUE_GETTER(is_ssa);

    /** @property uses_global_variable{get;}
     *  @brief    表示该指令是否直接使用全局状态作为操作数. */
    virtual bool uses_global_variable() const;

    /** @property uses_argument{get;}
     * @brief 是否直接使用了函数参数作为操作数 */
    bool uses_argument() const;

    /** @property ends_basic_block{get;default=false} abstract
     * @brief 遇到这个指令时是否应该结束它所在的基本块. 重载为`true`
     *        的指令结点类应当实现`IBasicBlockTerminator`接口. */
    bool ends_basic_block() const {
        return _instance_is_basicblock_terminator;
    }

    /** @fn unsafe_try_cast_to_terminator()
     * @brief 尝试把自己转换为 `IBasicBlockTerminator` 接口指针. 倘若自己是
     *        `IBasicBlockTerminator` 的基类, 则执行动态向上转换得到接口
     *        指针; 否则, 返回 null.
     *
     * @warning 该方法利用了 Itanium C++ ABI 中关于对象基类布局的规定, 通过存储
     *          并判断 `Instruction` 基类和 `IBasicBlockTerminator` 接口类
     *          起始地址的差异来做转换判断. 因此, 该方法原则上不支持采用非 Itanium
     *          ABI 的编译器. **倘若无视该警告, 则可能会产生不可预料的后果**.
     *
     * @warning [[特别注意]]该方法在 `this == null` 时会返回错误的结果, 这是
     *          C++ 语义规定导致的. C++ 规定 this == null 是未定义行为, 就算
     *          该方法内置 this == null 情况的特殊处理代码, 也可能会因为指针的
     *          nonnull 属性被优化掉, 导致处理代码失效. 如果你要保证结果完全
     *          正确, 请使用 `Instruction::TryCastToTerminator(this)` 静态方法. */
    IBasicBlockTerminator* unsafe_try_cast_to_terminator()
    {
        intptr_t offset = _ibasicblock_terminator_iface_offset;
        if (!ends_basic_block())
            return nullptr;
        intptr_t ithis  = reinterpret_cast<intptr_t>(this);
        intptr_t iiface = ithis + offset;
        return reinterpret_cast<IBasicBlockTerminator*>(iiface);
    }

    /** @fn unsafe_try_cast_to_terminator() const
     * @brief 尝试把不可变的自己转换为 `IBasicBlockTerminator` 不可变接口
     *        指针. 倘若自己是 `IBasicBlockTerminator` 的基类, 则执行动态向上
     *        转换得到接口指针; 否则, 返回 null. 该方法会调用 `this` 可变
     *        的 `unsafe_try_cast_to_terminator()` 方法.
     *
     * @warning 该方法利用了 Itanium C++ ABI 中关于对象基类布局的规定, 通过存储
     *          并判断 `Instruction` 基类和 `IBasicBlockTerminator` 接口类
     *          起始地址的差异来做转换判断. 因此, 该方法原则上不支持采用非 Itanium
     *          ABI 的编译器. **倘若无视该警告, 则可能会产生不可预料的后果**.
     *
     * @warning [[特别注意]]该方法在 `this == null` 时会返回错误的结果, 这是
     *          C++ 语义规定导致的. C++ 规定 this == null 是未定义行为, 就算
     *          该方法内置 this == null 情况的特殊处理代码, 也可能会因为指针的
     *          nonnull 属性被优化掉, 导致处理代码失效. 如果你要保证结果完全
     *          正确, 请使用 `Instruction::TryCastToTerminator(this)` 静态方法. */
    IBasicBlockTerminator const* unsafe_try_cast_to_terminator() const {
        return const_cast<Instruction*>(this)->unsafe_try_cast_to_terminator();
    }

    OVERRIDE_GET_TRUE(is_readable)
    OVERRIDE_GET_FALSE(is_writable)
    OVERRIDE_GET_TRUE(is_defined)

public: /* ==== [Instruction as List<BasicBlock>.Node] ==== */
    /** @property iterator{get;}
     * @brief 获取指向自身的迭代器.
     *
     * @warning `RefList<T>`类被重构过一次, 现在`Iterator`只有取元素的功能.
     *          修改自身周围的操作现在交给`Modifier`编辑迭代器完成, 原本使用
     *          `get_iterator()`方法做微操手术的做法现在应当换成`get_modifier()`
     *          方法.
     *
     * @see has_iterator()
     * @see get_modifier() */
    ListT::Iterator get_iterator() { return reflist_item_proxy().get_iterator(); }
    bool has_iterator() const noexcept { return get_parent() != nullptr; }

    /** @property modifier{get;}
     * @brief 获取自身的修改式迭代器
     *
     * 注意, Instruction不能自己删除自己, 所有在自身周围进行的增删查改
     * 操作都需要iterator辅助进行. iterator提供下面的操作:
     * - append: 在自己后面新增一个元素
     * - prepend: 在自己前面新增一个元素
     * - remove_this: 从列表中删除自己。如果想保留内存, 你需要在调用remove_this
     *          之前用一个owned指针保全自己的生命. */
    ListT::Modifier get_modifier() { return reflist_item_proxy().get_modifier(); }

    /** @property connect_status{get;set;}
     * @brief 指令结点的连接状态信息.
     * @see ConnectStatus 指令状态信息的枚举定义. */
    ConnectStatus get_connect_status() const noexcept { return _connect_status; }
    /** @fn set_connect_status()
     * @see connect_status{get;set;}
     * @warning 注意，直接修改 ConnectStatus 是非常危险的行为, 若操作不当
     *          很有可能引发内存泄漏, 或在没有错误的状态下异常退出等预期之外
     *          的行为.
     *          该修改接口是为 Instruction 连接管理工具类提供的. */
    void set_connect_status(ConnectStatus value) noexcept { _connect_status = value; }

    bool append(Instruction *next) noexcept(false) {
        return get_modifier().append(next);
    }
    bool prepend(Instruction *prev) noexcept(false) {
        return get_modifier().prepend(prev);
    }

    /** @property prev{get;}
     *  BasicBlock在维护子结点列表时一定要维护好prev与next两个结点. */
    Instruction *get_prev() const;

    /** @property next{get;} */
    Instruction *get_next() const;

    /** @fn unplugThis
     * @brief 无论是否被当作操作数使用, 都从BasicBlock中取下自己.
     *        假如自己被当成操作数，析构时会触发一个异常，让程序终止.
     * @warning 操作完成后, 状态变为`DISCONNECTED`. */
    RefT unplugThis() noexcept(false);

    /** @fn removeThisIfUnused
     * @brief 检查自己是否被当成操作数使用. 倘若不是，则从父结点中
     *        删除自己，否则返回nullptr.
     * @warning 操作完成后, 状态变为`FINALIZED`.
     * @return
     *  @retval this    自己可以被删除，返回清除后的自己
     *  @retval nullptr 自己无法被删除，返回nullptr. */
    RefT removeThisIfUnused() noexcept(false);

public: /* ===== [Instruction as Child of BasicBlock] ===== */
    /** @property parent{get;set;} */
    BasicBlock *get_parent()     const noexcept { return _parent; }
    void set_parent(BasicBlock *value) noexcept { _parent = value; }

    /** @property function{get;} 父结点的函数结点. */
    Function *get_function() const;

    /** @property module{get;}   自己所属的Module. */
    Module   *get_module() const;

    /** @fn isUsedOutsideOfParent() const
     * @brief 表示在所有以该指令为操作数的指令中, 是否有指令
     *        不在自己所在的基本块中; 是否有其他基本块的指令
     *        拿自己当操作数. */
    bool isUsedOutsideOfParent() const {
        return isUsedOutsideOfBlock(get_parent());
    }
    /** @fn isUsedOutsideOfBlock(const BasicBlock) const
     * @brief 表示在所有以该指令为操作数的指令中, 是否有指令
     *        不在参数所示的基本块中. */
    bool isUsedOutsideOfBlock(BasicBlock const* block) const;

    /** @fn isUsedOutsideOfBlocks(const Set<unowned BasicBlock>) const
     * @brief 表示在所有以该指令为操作数的指令中，是否有指令
     *        不在参数所示的基本块集合中 */
    bool isUsedOutsideOfBlocks(BasicBlockUSetT const &blocks) const;

public: /* ==== [Instruction as BasicBlock Life observer] ==== */
    ~Instruction() override;

protected:
    BasicBlock   *_parent;
    OpCode        _opcode;
    ConnectStatus _connect_status;
    /* type trait */
    bool          _instance_is_basicblock_terminator;
    int32_t       _ibasicblock_terminator_iface_offset;

    Instruction *_next, *_prev;
    ProxyT       _proxy;

    /** @fn constructor()
     * @brief 以最基本的初始化方式构造一个指令结点实例.
     *        由于 `Instruction` 是抽象类, 所以该构造函数是 `protected` 的.
     *
     * @warning `Instruction` 基类在构造时不会调用连接到 `BasicBlock` 的
     *          信号处理函数. `Instruction` 的子类需要在构造时自行处理连接
     *          信号. */
    Instruction(ValueTID tid,  Type *value_type,
                OpCode opcode, BasicBlock *parent = nullptr);

protected: /* ==== [Instruction as List<BasicBlock>.Node] ==== */
    /** @property reflist_item_proxy{unsafe access;} */
    ProxyT &reflist_item_proxy() { return _proxy; }

    /** @fn this => RefListProxy<Instruction>&
     * @brief `RefList<Instruction>` 的约束条件之一:
     *        必须有转换到 RefListProxy 的途径 */
    operator ProxyT&() { return _proxy; }

    /** @fn this => RefListProxy const& */
    operator ProxyT const&() const { return _proxy; }

public: /* ==== [Instruction as BasicBlock Life observer] ==== */
    /** @fn on_parent_plug(parent)
     * @brief signal 连接父结点时触发. 需要执行一次`_parent = parent`操作.
     *               要求 `parent` 不为 `null`. */
    virtual void on_parent_plug(BasicBlock *parent) {
        _parent = parent;
    }

    /** @fn on_parent_unplug()
     * @brief signal 在父结点断开时触发, 不允许清空_parent.
     * @return BasicBlock* 断开前的父结点 */
    virtual BasicBlock *on_parent_unplug() {
        _connect_status = ConnectStatus::DISCONNECTED;
        return _parent;
    }

    /** @fn on_parent_finalize()
     * @brief 在父结点析构前触发, 需要清空所有的操作数, 并把
     *        状态设置为`FINALIZED`.  */
    virtual void on_parent_finalize() = 0;

    /** @fn on_function_finalize()
     * @brief 在函数结点析构时触发, 需要清空所有操作数, 并把
     *        状态设置为`FINALIZED`. */
    virtual void on_function_finalize() = 0;
}; // imcomplete class Instruction


/* =================== [Instruction Actions] =================== */

/** @interface Instruction::ListAction
 * @brief 指令列表与列表迭代器执行时的信号接收器, 用于检查参数是否合格。
 *        倘若合格, 就返回true, 同时对参数做预处理. */
interface Instruction::ListAction: IRefListItemAction<Instruction> {
    using ElemT    = Instruction;
    /** `ListAction` 默认使用 `IRefListItemAction::Modifier`, 而我们实际需要的是
     *  `Instruction::ListT::Iterator`. 由于 C++ 没有协变/逆变的概念, 这两个
     *  `Modifier` 是完全无关的两个类, 与我们的预期不符. */
    using Modifier = Instruction::ListT::Modifier;
public:
    BasicBlock *old_parent = nullptr;
public: /* ==== [CRTP implements PreprocessorT] ==== */
    /** @fn on_modifier_append_preprocess
     *
     * @brief 检查结点的itreator和elem是否符合作为后继的要求.
     * @return bool 表示是否符合原地插入一个后继结点的条件. 若符合, 则返回 true,
     *         进行最后的附加和后处理操作; 否则, 中断附加操作
     *
     * 该预处理方法会缓存所在指令链表的公共父结点, 方便后处理时发出正确的
     * 父结点连接信号 `on_parent_plug()`. 倘若指令链表是空的, `old_parent`
     * 缓存就为空, 不进行任何后处理操作, 也不会发送任何信号.
     *
     * 当链表是空的, 结点满足下面所有规则时允许向后附加:
     * - 新结点是终止子
     * - 新结点处于断连 (DISCONNECTED) 状态, 既不连接, 也不析构.
     * - 迭代器不是链表的私有尾结点(不存放数据的, 由系统管理)
     *
     * 当链表不是空的, 结点满足下面所有规则时允许向后附加:
     * - 新结点不是终止子
     * - 新结点处于断连状态
     * - 新结点不是链表中的任意结点(上一条保证了新结点永远满足本条)
     * - - 迭代器不是链表的私有尾结点(不存放数据的, 由系统管理) */
    bool on_modifier_append_preprocess(Modifier *modifier, ElemT *inst);

    /** @fn on_modifier_prepend_preprocess
     * @brief 检查结点的itreator和elem是否符合作为前驱的要求.
     * @return bool 表示是否符合原地插入一个前驱结点的条件. 若符合, 则返回 true,
     *         进行最后的前向附加和后处理操作; 否则, 中断附加操作.
     *
     * 该预处理方法会缓存所在指令链表的公共父结点, 方便后处理时发出正确的
     * 父结点连接信号 `on_parent_plug()`. 倘若指令链表是空的, `old_parent`
     * 缓存就为空, 不进行任何后处理操作.
     *
     *
     * 当链表是空的, 结点满足下面所有规则时允许向前附加:
     * - 新结点是终止子
     * - 新结点处于断连 (DISCONNECTED) 状态, 既不连接, 也不析构.
     * - 迭代器不是链表的私有头结点(不存放数据的, 由系统管理)
     *
     * 当链表不是空的, 结点满足下面所有规则时允许向前附加:
     * - 新结点不是终止子
     * - 新结点处于断连状态
     * - 新结点不是链表中的任意结点(上一条保证了新结点永远满足本条)
     * - 迭代器不是链表的私有头结点(不存放数据的, 由系统管理) */
    bool on_modifier_prepend_preprocess(Modifier *modifier, ElemT *elem);

    /** @fn on_modifier_replace_preprocess
     * @brief 检查结点的itreator和elem是否符合原地替换的要求.
     * @return bool 表示是否符合原地替换为elem的条件. 若符合, 就返回 true, 进行
     *         最后的替换和替换后处理操作; 否则, 中断替换操作.
     *
     * 该方法会向将要脱落的原结点发出 `on_parent_unplug()` 父结点断连信号, 同时把
     * 父结点缓存在 `old_parent` 缓存中.
     *
     * 当结点满足下面所有规则时允许替换:
     * - 新旧结点不同
     * - 新结点的连接状态必须是 DISCONNECTED, 不能处于连接或析构状态
     * - 新旧结点要么都是基本块终止子, 要么都不是.  */
    bool on_modifier_replace_preprocess(Modifier *modifier, ElemT *new_inst);

    /** @fn on_modifier_disable_preprocess
     * @brief 检查结点的iterator是否满足删除的要求. 假定合格, 就触发
     *        `on_parent_unplug`信号, 然后返回`true`表示允许删除.
     * @return bool 表示是否符合删除结点的条件. 若符合, 则返回 true, 触发父结点断连
     *              信号, 删除结点; 否则返回 false, 终止删除操作.
     *
     * 当结点不是终止子时, 才允许删除. */
    bool on_modifier_disable_preprocess(Modifier *modifier);
public: /* ==== [CRTP implements SignalHandlerT] ==== */
    /** @brief 链表后附加操作的后处理函数.
     *
     * 当 old_parent 缓存不为空时会向 elem 发送 `on_parent_plug()` 信号,
     * 否则, 什么也不做. 此时, 你需要手动调用 on_parent_plug() 向 elem 发送
     * 父结点初始化信号. */
    void on_modifier_append(Modifier *modifier, ElemT *elem);

    /** @brief 链表前附加操作的后处理函数.
     *
     * 当 old_parent 缓存不为空时会向 elem 发送 `on_parent_plug()` 信号,
     * 否则, 什么也不做. 此时, 你需要手动调用 on_parent_plug() 向 elem 发送
     * 父结点初始化信号. */
    void on_modifier_prepend(Modifier *modifier, ElemT *elem);

    /** @brief 链表原地替换的后处理函数.
     *
     * 不出错的情况下, old_parent 总是非空的, 因此向 modifier 指向的新
     * 结点发送一个 `on_parent_plug()` 信号处理函数.
     * 否则报错, 可能会强制终止应用程序, 也可能什么也不做. */
    void on_modifier_replace(Modifier *modifier, ElemT *old_self);
}; // interface Instruction::ListAction

/** @interface IBasicBlockTerminator
 *  @brief 基本块终止指令的共同基础接口. 实现了该接口的指令,
*          `ends_basic_block` 属性必须覆盖为 `true`. */
interface IBasicBlockTerminator {
    using TraverseFunc    = bool(*)(BasicBlock*);
    using TraverseClosure = std::function<bool(BasicBlock*)>;

    /** @fn ReplaceClosure(old, out new_target) => bool
     * @brief 判断是否需要进行替换操作的闭包.
     *
     * @param old        传入的旧 `BasicBlock` 引用.
     * @param new_target 传出参数. 传出 `null` 时表示跳过该 `BasicBlock`, 而非替换
     *                   为 `null`. 传出与 `old` 相同的值时, 也表示跳过该 `BasicBlock`.
     * @return 要求返回一个 `bool` 常量, 表示是否需要终止替换操作. */
    using ReplaceClosure  = std::function<bool(BasicBlock *old, BasicBlock *&out_new_target)>;

    /** @struct TraitOwner
     * @brief IBasicBlockTerminator 持有所有权的动态接口句柄, 但是没有指针功能.
     * @fn get()       返回持有所有权的 `terminator` 指针.
     * @fn get_iface() 返回 `IBasicBlockTerminator` 接口指针. */
    struct TraitOwner {
    private:
        owned<Instruction>     instance;
        IBasicBlockTerminator *iface;
    public:
        TraitOwner(owned<Instruction> inst, IBasicBlockTerminator* iface)
            : instance(std::move(inst)), iface(iface) {}

        /** @brief 从接口指针构造, 要求接口指针不为 `null`. */
        TraitOwner(IBasicBlockTerminator *terminator)
            : instance(terminator->get_instance()),
              iface(terminator) {}

        TraitOwner(TraitOwner const&) = default;
        TraitOwner(TraitOwner &&) = default;
        TraitOwner(): iface(nullptr) {}

        template<typename TerminatorT>
        MTB_REQUIRE((std::is_base_of_v<IBasicBlockTerminator, TerminatorT> &&
                     std::is_base_of_v<Instruction, TerminatorT>))
        TraitOwner(TerminatorT *terminator)
            : instance(terminator),
              iface(static_cast<IBasicBlockTerminator*>(terminator)) {
        }

        operator Instruction*()   const& { return instance; }
        operator owned<Instruction>() && {
            owned<Instruction> ret = std::move(instance);
            return ret;
        }

        TraitOwner &operator=(std::nullptr_t);
        TraitOwner &operator=(Instruction *inst);
        template<typename TerminatorT>
        MTB_REQUIRE((std::is_base_of_v<IBasicBlockTerminator, TerminatorT> &&
                     std::is_base_of_v<Instruction, TerminatorT>))
        TraitOwner &operator=(owned<TerminatorT> inst);

        Instruction *get() const { return instance; }
        IBasicBlockTerminator *get_iface() const {
            return iface;
        }
    }; // struct TraitPtr

    class BadBaseException;
protected:
    IBasicBlockTerminator() = default;
    IBasicBlockTerminator(Instruction* self)
        : iterminator_virtual_this(self) {}
    virtual ~IBasicBlockTerminator();
public:
    /** @fn traverse_targets((BasicBlock) => bool)
     * @brief 令 `target` 为子类中的任意跳转目标属性, 则遍历读取
     *        或修改所有 `target` 直到表达式 `fn(target)` 为真.
     *        该函数接受函数指针形式的`fn`.
     * @warning 重申一遍, `fn` 返回 `true` 时结束遍历. */
    abstract void traverse_targets(TraverseFunc    fn) = 0;

    /** @fn traverse_targets((BasicBlock) => bool)
     * @brief 令 `target` 为子类中的任意跳转目标属性, 则遍历读取
     *        或修改所有 `target` 直到表达式 `fn(target)` 为真.
     *        该函数接受闭包形式的`fn`.
     * @warning 重申一遍, `fn` 返回 `true` 时结束遍历. */
    abstract void traverse_targets(TraverseClosure fn) = 0;

    /** @fn remove_target
     * @brief 当参数所示的 `target` 与子类的跳转目标属性相等时,
     *        删除该跳转目标. */
    abstract void remove_target(BasicBlock *target)    = 0;

    /** @fn replace_target(old -> new)
     * @brief 遍历所有跳转目标, 把遇到的所有等于 `old` 的跳转目标替换
     *        为 `new_target`.
     * @warning new_target 不能为 null, 否则遇到动态增删的跳转目标时
     *          会抛出 NullException. */
    abstract void replace_target(BasicBlock *old, BasicBlock *new_target) = 0;

    /** @fn replace_target_once(old -> new)
     * @brief 遍历所有跳转目标, 把遇到的第一个等于 `old` 的跳转目标替换
     *        为 `new_target`.
     *
     * 该方法会产生一个闭包并调用 `replace_target_by` 方法, 所以不需要
     * 子类做重写. */
    void replace_target_once(BasicBlock *old, BasicBlock *new_target);

    /** @fn replace_target_by((old, out new) => terminates?)
     * @brief 遍历所有跳转目标, 把每个跳转目标输入闭包 `fn` 进行判断.
     *        判断闭包的书写规则请参见 `ReplaceClosure` 的定义.
     *
     * 判断规则是:
     * - 若 `fn` 返回 `true`, 则立即终止遍历;
     * - 若 `fn` 输出参数为与 `old` 相同的值或 `null`, 则不替换;
     * - 否则, 把 `old` 位置的跳转目标替换为 `new` 的. */
    abstract void replace_target_by(ReplaceClosure fn) = 0;

    /** @fn remove_target_if
     * @brief 令 `target` 为子类中的任意跳转目标属性, 当表达式
     *        `fn(target)` 为 `true` 时, 删除该跳转目标.
     *        该函数接受函数指针形式的`fn`, 以减少开销. */
    abstract void remove_target_if(TraverseFunc    fn) = 0;

    /** @fn remove_target_if
     * @brief 令 `target` 为子类中的任意跳转目标属性, 当表达式
     *        `fn(target)` 为 `true` 时, 删除该跳转目标.
     *        该函数接受闭包形式的 `fn`. */
    abstract void remove_target_if(TraverseClosure fn) = 0;

    /** @fn clean_targets()
     * @brief 清除所有跳转目标. 与其他所有 `clean` 方法一样,
     *        静态存储的跳转目标变为 `null`, 动态分配的跳转目标
     *        会随着辅助数据块直接删除. */
    abstract void clean_targets() = 0;

    Instruction *get_instance() const {
        return iterminator_virtual_this;
    }
    TraitOwner get_trait() {
        return TraitOwner{iterminator_virtual_this, this};
    }
protected:
    Instruction *iterminator_virtual_this;
}; // interface BasicBlockTerminator

/** @class OpCodeMismatchException
 *  @brief 操作码不匹配 */
class Instruction::OpCodeMismatchException: public MTB::Exception {
public:
    OpCodeMismatchException(OpCode      err_opcode,
                            std::string mismatch_reason,
                            std::string additional_info = "",
                            SourceLocation src_location = CURRENT_SRCLOC);
    OpCode      opcode;
    std::string mismatch_reason;
}; // class Instruction::OpCodeMismatchException


class IBasicBlockTerminator::BadBaseException: public MTB::Exception {
public:
    BadBaseException(Instruction *instance, std::string &&additional_msg, SourceLocation srcloc);

    Instruction *instance;
    std::string  additional_msg;
}; // class IBasicBlockTerminator::BadBaseException

} // namespace MYGL::IR

/* =================================[impl]================================= */
namespace MYGL::IR {

/** @class Instruction */
constexpr inline bool
Instruction::OpCode::is_integer_op() const noexcept
{
    if (is_logical_op()) return true;

    switch (self) {
    case ZEXT: case SEXT: case TRUNC: case INEG: case NOT:
    case ADD:  case SUB:  case MUL:   case SDIV: case UDIV:
    case ICMP:
                return true;
    default:    return false;
    } // switch (self)
}
constexpr inline bool
Instruction::OpCode::operand_requires_int() const noexcept
{
    return is_integer_op() ||
           self == ITOF    || self == UTOF;
}

constexpr inline bool
Instruction::OpCode::is_float_op() const noexcept
{
    switch (self) {
    case FPEXT: case FPTRUNC:
    case FNEG:
    case FADD: case FSUB:  case FMUL: case FDIV:
    case FREM: case FCMP:
                return true;
    default:    return false;
    }
}

constexpr inline bool
Instruction::OpCode::operand_requires_float() const noexcept
{
    return is_float_op() || self == FTOI;
}

constexpr inline bool
Instruction::OpCode::is_cast_op() const noexcept {
    return self >= ITOF && self <= FPTRUNC;
}

constexpr inline bool
Instruction::OpCode::is_logical_op() const noexcept
{
    bool result = ((self >= AND) & (self <= ASHR));
    return result;
}

constexpr inline bool
Instruction::OpCode::is_swappable() const noexcept {
    bool result = is_add()    || is_mul()   ||
                  self == AND || self == OR || self == XOR ||
                  self == PHI;
    return result;
}

/** =============== [ @class IBasicBlockTerminator ] ================ */
inline IBasicBlockTerminator::TraitOwner &
IBasicBlockTerminator::TraitOwner::operator=(std::nullptr_t) {
    instance = nullptr, iface = nullptr;
    return *this;
}

template<typename TerminatorT>
MTB_REQUIRE((std::is_base_of_v<IBasicBlockTerminator, TerminatorT> &&
             std::is_base_of_v<Instruction, TerminatorT>))
IBasicBlockTerminator::TraitOwner &
IBasicBlockTerminator::TraitOwner::operator=(owned<TerminatorT> terminator)
{
    if (terminator == instance)
        return *this;
    instance = owned<Instruction>(std::move(terminator));
    iface    = static_cast<IBasicBlockTerminator*>(terminator.get());
    return *this;
}

/* end impl */

} // namespace MYGL::IR

namespace std {
    template<>
    struct hash<MYGL::IR::Instruction::OpCode> {
        using elem_type = MYGL::IR::Instruction::OpCode;
        size_t operator()(elem_type elem) const noexcept {
            return elem.hash();
        }
    }; // struct hash<MYGL::IR::Instruction::OpCode>
}

#define MYGL_IBASICBLOCK_TERMINATOR_OFFSET(self) \
    (reinterpret_cast<intptr_t>(static_cast<IBasicBlockTerminator*>(self))\
     - reinterpret_cast<intptr_t>(static_cast<Instruction*>(self)))
#define MYGL_IBASICBLOCK_TERMINATOR_INIT_OFFSET() do {\
        _instance_is_basicblock_terminator = true; \
        _ibasicblock_terminator_iface_offset = \
        MYGL_IBASICBLOCK_TERMINATOR_OFFSET(this); \
    } while (false)

#endif
