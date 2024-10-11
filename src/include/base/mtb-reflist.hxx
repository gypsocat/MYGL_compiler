#pragma once
#include <stdexcept>
#ifndef __MTB_REFLIST_H__
#define __MTB_REFLIST_H__

#include "mtb-object.hxx"
#include "mtb-exception.hxx"
#include "mtb-compatibility.hxx"
#include "mtb-own-macro.hxx"
#include <functional>
#include <type_traits>
#include <string>
#include <utility>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <string_view>

namespace MTB {

    template<PublicExtendsObject ElemT, typename ItemActionT>
    class RefList;

    template<typename ElemT, typename ActionT>
    struct RefListProxy;

    template<PublicExtendsObject ElemT>
    interface IRefListItemPreprocess;
    
    template<PublicExtendsObject ElemT>
    interface IRefListItemSigHandler;

    template<typename ElemT,
             typename PreprocessorT  = IRefListItemPreprocess<ElemT>,
             typename SignalHandlerT = IRefListItemSigHandler<ElemT>>
    interface IRefListItemAction;

    /** @class RefList
    * @brief 为MTB::Object的子类设计的组合式双向链表, 经过合理设计的元素可以根据自身的地址
    *        快速生成一个指向自己的迭代器，用于在自身周围做增删查改等“微操手术”。
    *
    * @param ElemT 元素类型. 约束:
    *   1. 是Object的子类
    *   2. 必须有`reflist_item_proxy`这个属性.
    * @param ItemActionT 迭代访问检查器。约束: 要求拥有`IRefListItemAction`模板接口的所有方法，
    *        `Iterator`在进行“微操手术”时需要调用这些方法做检查。
    *        自定义一个`ItemActionT`接口最快的方法是继承`IRefListItemAction<ElemT>`. */
    template<PublicExtendsObject ElemT,
             typename ItemActionT = IRefListItemAction<ElemT>>
    class RefList: public MTB::Object {
    public:
        struct Iterator;
        struct RIterator;
        struct ConstIterator;
        struct Modifier;
        struct ModifyRange;
        class  WrongItemException;

        using size_type  = size_t;
        using ProxyT     = RefListProxy<ElemT, ItemActionT>;
        using AccessFunc = std::function<bool(ElemT &)>;
        using iterator   = Iterator;
        using const_iterator = ConstIterator;

        friend ElemT;
        friend ProxyT;
        friend struct Iterator;
        friend struct RIterator;
        friend struct Modifier;
        friend ItemActionT;
    protected:
        /** @struct Node
         * @brief 链表结点. */
        struct Node {
            Node *prev, *next;
            RefList   *parent;
            owned<ElemT> elem;
        public:
            bool node_begins() const { return prev == nullptr; }
            bool node_ends()   const { return next == nullptr; }
            bool instance_begins() const {
                return node_begins() || prev->prev == nullptr;
            }
            bool instance_ends() const {
                return node_ends()   || next->next == nullptr;
            }
        }; // struct Node
    public:
        explicit RefList() {
            _node_begin = new Node{nullptr, nullptr, this, nullptr};
            _node_end   = new Node{nullptr, nullptr, this, nullptr};
            _node_begin->next = _node_end;
            _node_end->prev   = _node_begin;
            _length     = 0;
        }
        explicit RefList(RefList &&another)
            : RefList() {
            std::swap(_node_begin->next, another._node_begin->next);
            std::swap(_node_end->prev,   another._node_end->prev);
            std::swap(_length, another._length);
        }
        ~RefList() {
            clean();
            delete _node_begin;
            delete _node_end;
        }

        /* ===== [Property of RefList<E, A>] ===== */
        /** @property length{get;} */
        size_type get_length() const { return _length; }
        size_type size() const { return _length; }
        bool empty() const {
            return _node_begin->next == _node_end;
        }

        /* ==== [Traverse Method of RefList] ==== */
        Iterator begin() {
            return Iterator{nodeof_front(), this};
        }
        Iterator end() {
            return Iterator{_node_end, this};
        }
        RIterator rbegin() {
            return RIterator{nodeof_back(), this};
        }
        RIterator rend() {
            return RIterator{_node_begin, this};
        }
        ConstIterator cbegin() const {
            return ConstIterator{nodeof_front()};
        }
        ConstIterator cend() const {
            return ConstIterator{_node_end};
        }
        ConstIterator begin() const { return cbegin(); }
        ConstIterator end()   const { return cend(); }

        /* ====== [Locate RefList Item] ====== */
        Iterator iterator_at(size_t index);
        Modifier modifier_at(size_type index) {
            return Modifier{iterator_at(index)};
        }
        owned<ElemT> &at(size_t index) {
            Iterator it = iterator_at(index);
            Node  *node = it.node;
            return node->elem;
        }
        owned<ElemT> &front() {
            if (empty()) {
                throw EmptySetException("RefList.front()",
                    "cannot get first item from an empty list");
            }
            return nodeof_front()->elem;
        }
        owned<ElemT> &back() {
            if (empty()) {
                throw EmptySetException("RefList.back()",
                    "cannot get last item from an empty list");
            }
            return _node_end->prev->elem;
        }
        owned<ElemT> const& front() const {
            if (empty()) {
                throw EmptySetException("RefList",
                    "cannot get first item from an empty list");
            }
            return nodeof_front()->elem;
        }
        owned<ElemT> const& back() const {
            if (empty()) {
                throw EmptySetException("RefList",
                    "cannot get last item from an empty list");
            }
            return nodeof_back()->elem;
        }

        /* ==== [List Operations] ==== */
        bool append(owned<ElemT> element)
        {
            Modifier mod_back{nodeof_back(), this};
            return mod_back.append(std::move(element));
        }
        void push_back(owned<ElemT> element) { append(std::move(element)); }

        template<typename ElemET, typename...Args>
        MTB_REQUIRE ((std::is_base_of_v<ElemT, ElemET>))
        ElemT *emplace_back(Args...args)
        {
            owned<ElemET> elem = own<ElemET>(std::forward<Args>(args)...);
            ElemT *ret = elem.get();
            append(std::move(elem));
            return ret;
        }
        template<typename ElemET>
        MTB_REQUIRE ((std::is_base_of_v<ElemT, ElemET>))
        ElemT *emplace_back()
        {
            MTB_NEW_OWN(ElemET, elem);
            ElemT *ret = elem.get();
            append(std::move(elem));
            return ret;
        }

        void prepend(owned<ElemT> element)
        {
            Modifier mod_f = {_node_begin->next, this};
            return mod_f.prepend(std::move(element));
        }
        void push_front(owned<ElemT> element) { prepend(std::move(element)); }
        template<typename ElemET, typename...Args>
        MTB_REQUIRE ((std::is_base_of_v<ElemT, ElemET>))
        ElemT *emplace_front(Args...args) {
            ElemT *ret = new ElemET(std::forward(args...));
            prepend(ret);
            return ret;
        }

        owned<ElemT> pop_front()
        {
            Modifier mod_f = {nodeof_front(), this};
            return mod_f.remove_this();
        }
        owned<ElemT> pop_back()
        {
            Modifier mod_b = back();
            return mod_b.remove_this();
        }

        void clean();
    protected:
        Node  *_node_begin, *_node_end;
        size_t _length;
    protected:
        Node *nodeof_front() const { return _node_begin->next; }
        Node *nodeof_back()  const { return _node_end->prev; }
    }; // class RefList<E, A>


//==============[Iterators & Modifiers 迭代器和修改器]================//

    /** @struct RefList<E, A>::Iterator
     *  @brief 一个简单的遍历式迭代器，是其他迭代器 & 修改器的基类 */
    template<PublicExtendsObject ElemT, typename ItemActionT>
    struct RefList<ElemT, ItemActionT>::Iterator {
        friend ItemActionT;
        using  ListT = RefList<ElemT, ItemActionT>;
        using  RefT  = owned<ElemT>;
    public:
        Node   *node;
        ListT  *list;
    public:
        bool begins() const { return node->instance_begins(); }
        bool ends()   const { return node->instance_ends(); }

        bool node_begins() const { return node->node_begins(); }
        bool node_ends()   const { return node->node_ends(); }

        bool is_available() const {
            return (node != nullptr) & (list != nullptr);
        }

        /* ==== [Item Getter] ====*/
        RefT &      get() { return node->elem; }
        RefT const& get() const { return node->elem; }
        RefT &      operator*() { return get(); }
        RefT const& operator*() const { return get(); }
        ElemT *      operator->() { return get().get(); }
        ElemT const* operator->() const { return get().get(); }

        /* ==== [Iter Advance] ====*/
        Iterator &operator++() { advance_fwd_unsafe(); return *this; }
        Iterator operator++(int) {
            Iterator ret{*this};
            advance_fwd();
            return ret;
        }
        Iterator &operator--() { advance_backwd_unsafe(); return *this; }
        Iterator operator--(int) {
            Iterator ret{*this};
            advance_backwd();
            return ret;
        }
        bool operator!=(Iterator const &another) const {
            return node != another.node;
        }

        /* ==== [Basic Get method] ==== */
        Iterator get_next_iterator() const
        {
            Iterator ret = *this;
            if (!ret.advance_fwd())
                return {nullptr, nullptr};
            return ret;
        }
        Iterator get_prev_iterator() const
        {
            Iterator ret = *this;
            if (!ret.advance_backwd())
                return {nullptr, nullptr};
            return ret;
        }

    protected:
        bool advance_fwd()
        {
            if (node->node_ends())
                return false;
            node = node->next;
            return true;
        }
        bool advance_backwd()
        {
            if (node->node_begins())
                return false;
            node = node->prev;
            return true;
        }
        void advance_fwd_unsafe()    { node = node->next; }
        void advance_backwd_unsafe() { node = node->prev; }
    }; // struct RefList<ElemT, ItemActionT>::Iterator


    /** @struct RIterator
     *  @brief  反向迭代器 */
    template<PublicExtendsObject ElemT, typename ItemActionT>
    struct RefList<ElemT, ItemActionT>::RIterator: public Iterator {
        using Iterator::Iterator;
        RIterator(Node *node, Iterator::ListT *list)
            : Iterator(node, list){
        }
        /* ==== [Iter Advance] ====*/
        RIterator &operator++() { return Iterator::operator--(); }
        RIterator &operator--() { return Iterator::operator++(); }
        Iterator operator++(int) {
            Iterator ret{*this};
            Iterator::advance_backwd();
            return ret;
        }
        Iterator operator--(int) {
            Iterator ret{*this};
            Iterator::advance_fwd();
            return ret;
        }
    }; // struct RefList<ElemT, ItemActionT>::RIterator

    /** @struct Modifier
     * @brief RefList的微操手术刀
     *
     * Modifier提供如下功能:
     * - `append`/`push_back`: 在自己后面附加一个元素
     * - `prepend`/`push_front`: 在自己前面附加一个元素
     * - `remove_this`: 删除自己，然后禁用该修改器
     * - `replace_this`: 把自己替换成别的 */
    template<PublicExtendsObject ElemT, typename ItemActionT>
    struct RefList<ElemT, ItemActionT>::Modifier: public Iterator {
        using ProxyT = RefListProxy<ElemT, ItemActionT>;
        using RefT   = owned<ElemT>;
        using Iterator::Iterator;

        Modifier(Iterator const &it) {
            Iterator::list = it.list;
            Iterator::node = it.node;
        }
        Modifier(Node *node, Iterator::ListT *list)
            : Iterator(node, list){
        }
    public:
        bool append(RefT elem);
        bool push_back(RefT elem)  { return append(std::move(elem)); }

        bool prepend(RefT elem);
        bool push_front(RefT elem) { return prepend(std::move(elem)); }

        template<typename ElemET, typename...Args>
        MTB_REQUIRE((std::is_base_of_v<ElemT, ElemET>))
        bool emplace_back(Args...args) {
            return append(new ElemET(args...));
        }
        template<typename ElemET, typename...Args>
        MTB_REQUIRE((std::is_base_of_v<ElemT, ElemET>))
        bool emplace_front(Args...args) {
            return prepend(new ElemET(args...));
        }

        RefT remove_this();
        RefT replace_this(RefT another);
    public: /* operations with no check */
        RefT &unsafe_expose_reference() {
            return Iterator::node->elem;
        }

        void disable() {
            this->list = nullptr;
            this->node = nullptr;
        }
    }; // struct RefList<ElemT, ItemActionT>::Modifier

//=====================[ConstIterator: 只读迭代器]======================//

    template<PublicExtendsObject ElemT, typename ItemActionT>
    struct RefList<ElemT, ItemActionT>::ConstIterator {
        using RefT = owned<ElemT>;
        Node const* node;
    public:
        ConstIterator(Node const* node): node(node) {}
        ConstIterator(ConstIterator const&) = default;
        ConstIterator(Iterator const &it) : node(it.node){}
    public:
        /* ==== [Item Getter] ====*/
        RefT  const& get()        const { return node->elem; }
        RefT  const& operator*()  const { return get(); }
        ElemT const* operator->() const { return get(); }

        /* ==== [Iter Advance] ====*/
        ConstIterator &operator++() { advance_fwd_unsafe(); return *this; }
        ConstIterator operator++(int) {
            ConstIterator ret{*this};
            advance_fwd();
            return ret;
        }
        ConstIterator &operator--() { advance_backwd_unsafe(); return *this; }
        ConstIterator operator--(int) {
            ConstIterator ret{*this};
            advance_backwd();
            return ret;
        }
        bool operator!=(ConstIterator const &another) const {
            return node != another.node;
        }
        bool operator==(ConstIterator const &another) const {
            return node == another.node;
        }

        bool advance_fwd()
        {
            if (node->node_ends())
                return false;
            node = node->next;
            return true;
        }
        bool advance_backwd()
        {
            if (node->node_begins())
                return false;
            node = node->prev;
            return true;
        }
    protected:
        void advance_fwd_unsafe()    { node = node->next; }
        void advance_backwd_unsafe() { node = node->prev; }
    }; // struct RefList<ElemT, ItemActionT>::ConstIterator

//=============[ModifyRange: 遍历RefList生成Modifier的工具]==============//
    /** @struct ModifyRange */
    template<PublicExtendsObject ElemT, typename ItemActionT>
    struct RefList<ElemT, ItemActionT>::ModifyRange {
        Modifier __begin, __end;
    public:
        ModifyRange() = delete;
        ModifyRange(RefList &list) noexcept {
            __begin = list.begin();
            __end   = list.end();
        }

        Modifier begin() const { return __begin; }
        Modifier end()   const { return __end; }
    }; // struct RefList<ElemT, ItemActionT>::ModifyRange


//=============[Proxy: RefList的结点代理, 是ElemT的父类之一]==============//

    /** @struct Proxy
     *  @brief RefList的结点代理, 是ElemT的父类之一 */
    template<typename ElemT, typename ItemActionT = IRefListItemAction<ElemT>>
    struct RefListProxy {
        using ListT    = RefList<ElemT, ItemActionT>;
        using NodeT    = typename ListT::Node;
        using Iterator = typename ListT::Iterator;
        using Modifier = typename ListT::Modifier;
    public:
        RefListProxy &reflist_item_proxy() const { return this; }

        Iterator get_iterator() const {
            if (self_node == nullptr)
                return {nullptr, nullptr};
            return Iterator{self_node, self_node->parent};
        }
        Modifier get_modifier() const { return Modifier{get_iterator()}; }
    public:
        NodeT *self_node = nullptr;
    }; // struct RefListProxy

//==============[RefListItemAction: RefList的信号接收器]=================//

    /** @interface IRefListItemPreprocess<ElemT>
     *  @brief RefList::Modifier的预处理器, 负责处理检查-预处理信号 */
    template<PublicExtendsObject ElemT>
    interface IRefListItemPreprocess {
        template<typename ItemActionT>
        using Modifier = typename RefList<ElemT, ItemActionT>::Modifier;
    public:
        template<typename ItemActionT>
        bool on_modifier_append_preprocess(Modifier<ItemActionT> *modifier,
                                           ElemT *elem) { return true; }
        template<typename ItemActionT>
        bool on_modifier_prepend_preprocess(Modifier<ItemActionT> *modifier,
                                            ElemT *elem) { return true; }
        template<typename ItemActionT>
        bool on_modifier_replace_preprocess(Modifier<ItemActionT> *modifier,
                                            ElemT *new_self) { return true; }
        template<typename ItemActionT>
        bool on_modifier_disable_preprocess(Modifier<ItemActionT> *modifier) {
            (void)modifier;
            return true;
        }
    }; // interface IRefListItemPreprocess<ElemT>


    /** @interface IRefListItemSigHandler<ElemT>
     *  @brief RefList::Modifier的信号处理器, 负责处理修改后的信号 */
    template<PublicExtendsObject ElemT>
    interface IRefListItemSigHandler {
        template<typename ItemActionT>
        using Modifier = typename RefList<ElemT, ItemActionT>::Modifier;
    public:
        template<typename ItemActionT>
        void on_modifier_append(Modifier<ItemActionT> *modifier,
                                ElemT                 *elem) {}
        template<typename ItemActionT>
        void on_modifier_prepend(Modifier<ItemActionT> *modifier,
                                 ElemT                 *elem) {}
        template<typename ItemActionT>
        void on_modifier_replace(Modifier<ItemActionT> *modifier,
                                 ElemT                 *old_self) {}
    }; // interface IRefListItemSigHandler<ElemT>


    /** @interface IRefListItemAction<E, P, S>
     *  @brief RefList默认的信号总成. 使用CRTP实现了预处理器和信号接收器所需的方法. */
    template<typename ElemT, typename PreprocessorT, typename SignalHandlerT>
    interface IRefListItemAction: public PreprocessorT,
                                  public SignalHandlerT {
        using Modifier = typename RefList<ElemT, IRefListItemAction>::Modifier;
    public: /* ==== [CRTP implements PreprocessorT] ==== */
        bool on_modifier_append_preprocess(Modifier *modifier, ElemT *elem) {
            return PreprocessorT::template
                on_modifier_append_preprocess<IRefListItemAction>(modifier, elem);
        }
        bool on_modifier_prepend_preprocess(Modifier *modifier, ElemT *elem) {
            return PreprocessorT::template
                on_modifier_prepend_preprocess<IRefListItemAction>(modifier, elem);
        }
        bool on_modifier_replace_preprocess(Modifier *modifier, ElemT *ns) {
            return PreprocessorT::template
                on_modifier_replace_preprocess<IRefListItemAction>(modifier, ns);
        }
        bool on_modifier_disable_preprocess(Modifier *modifier) {
            return PreprocessorT::template
                on_modifier_disable_preprocess<IRefListItemAction>(modifier);
        }
    public: /* ==== [CRTP implements SignalHandlerT] ==== */
        void on_modifier_append(Modifier *modifier, ElemT *elem) {
            return SignalHandlerT::template
                on_modifier_append<IRefListItemAction>(modifier, elem);
        }
        void on_modifier_prepend(Modifier *modifier, ElemT *elem) {
            return SignalHandlerT::template
                on_modifier_prepend<IRefListItemAction>(modifier, elem);
        }
        void on_modifier_replace(Modifier *modifier, ElemT *old_self) {
            return SignalHandlerT::template
                on_modifier_replace<IRefListItemAction>(modifier, old_self);
        }
    }; // interface IRefListItemAction<E, P, S>

/* ================ [WrongItemException:列表元素出错异常] ================ */
    namespace RefListImpl {
        std::string w5i4e9_make_msg(void *list, void *item, uint8_t code,
                                    std::string const& msg, SourceLocation srcloc);
        class GenericWrongItemException: public Exception {
        public:
            enum ErrorCode: uint8_t {
                UNEXIST, WRONG_LIST
            }; // enum ErrorCode: uint8_t
        public:
            GenericWrongItemException(Object *list, Object *item, ErrorCode code,
                                      std::string const& msg, SourceLocation srcloc)
            : MTB::Exception(ErrorLevel::CRITICAL,
                             RefListImpl::w5i4e9_make_msg(list, item, code, msg, srcloc),
                             CURRENT_SRCLOC_F),
              _list(list), _item(item), internal_msg(msg) {}
        public:
            Object *_list;
            Object *_item;
            std::string internal_msg;
        }; // class WrongItemExceptionBase
    } // namespace RefListImpl

    template<PublicExtendsObject ElemT, typename ProxyT>
    class RefList<ElemT, ProxyT>::WrongItemException
        : public RefListImpl::GenericWrongItemException {
    public:
        WrongItemException(RefList *list, ElemT *item, ErrorCode code,
                           std::string const& msg, SourceLocation srcloc)
            : RefListImpl::GenericWrongItemException {
                list, item, code, msg, srcloc
              } {}
    public:
        RefList *get_list() const {
            return static_cast<RefList*>(_list);
        }
        ElemT *get_item() const {
            return static_cast<ElemT*>(_item);
        }
    }; // class RefList<ElemT, ProxyT>::ItemUnexistException
} // namespace MTB


/* ========== [Impl for RefList<E, I>] ==========*/
namespace MTB {

    template<PublicExtendsObject ElemT, typename ItemAction>
    typename RefList<ElemT, ItemAction>::Iterator
    RefList<ElemT, ItemAction>::iterator_at(size_type index)
    {
        if (empty()) {
            throw EmptySetException("RefList.iterator_at()",
                "you get none of iterators in an empty list");
        }
        if (index >= get_length()) {
            std::string msg = "The list has ";
            msg += std::to_string(size());
            msg += " elements while you want to get element ";
            msg += std::to_string(index);
            throw std::out_of_range(std::move(msg));
        }
        Node *i = _node_begin->next;
        int cnt = 0;
        while (cnt < index && i != _node_end) {
            i = i->next;
            cnt++;
        }
        if (i == _node_end) {
            _length = cnt;
            throw std::out_of_range("iterator_at() overflow and index error");
        }
        return {i, this};
    }

    template<PublicExtendsObject ElemT, typename ItemAction>
    void RefList<ElemT, ItemAction>::clean()
    {
        Node *i = nodeof_front();
        while (i != _node_end) {
            Node *prev = i;
            i = i->next;
            i->elem.reset();
            delete prev;
        }
        _node_begin->next = _node_end;
        _node_end->prev = _node_begin;
    }
} // namespace MTB

/* ========== [Impl for RefList<E, I>::Modifier] ========== */
namespace MTB {
#   ifdef _M_ModifierT
#   error _M_ModifierT should be undefined.
#   endif
#   define _M_ModifierT RefList<ElemT, ItemActionT>::Modifier

    template<PublicExtendsObject ElemT, typename ItemActionT>
    bool _M_ModifierT::append(RefT elem)
    {
        ItemActionT a;
        if (Iterator::node->node_ends() ||
            elem == this->get() ||
            a.on_modifier_append_preprocess(this, elem) == false)
            return false;
        ElemT *pelem = elem;
        Node  *curr = Iterator::node;
        Node  *next = curr->next;
        Node  *new_node = new Node{curr, next, curr->parent, std::move(elem)};
        ProxyT &proxy   = pelem->reflist_item_proxy();
        proxy.self_node = new_node;
        curr->next      = new_node;
        next->prev      = new_node;
        Iterator::list->_length++;
        a.on_modifier_append(this, pelem);
        return true;
    }

    template<PublicExtendsObject ElemT, typename ItemActionT>
    bool _M_ModifierT::prepend(RefT elem)
    {
        ItemActionT a;
        if (Iterator::node->node_begins())
            return false;
        if (a.on_modifier_prepend_preprocess(this, elem) == false)
            return false;
        ElemT *pelem = elem;
        Node  *curr = Iterator::node;
        Node  *prev = curr->prev;
        Node  *new_node = new Node{prev, curr, curr->parent, std::move(elem)};
        ProxyT &proxy   = pelem->reflist_item_proxy();
        proxy.self_node = new_node;
        curr->prev      = new_node;
        prev->next      = new_node;
        Iterator::list->_length++;
        a.on_modifier_prepend(this, pelem);
        return true;
    }

    template<PublicExtendsObject ElemT, typename ItemActionT>
    owned<ElemT> _M_ModifierT::remove_this()
    {
        ItemActionT a;
        MTB_UNLIKELY_IF (Iterator::node == nullptr   ||
                         Iterator::node->node_ends() ||
                         Iterator::node->node_begins())
            return nullptr;
        
        if (a.on_modifier_disable_preprocess(this) == false)
            return nullptr;

        Node *curr = Iterator::node;
        Node *next = curr->next;
        Node *prev = curr->prev;
        next->prev = prev;
        prev->next = next;
        RefT ret{std::move(curr->elem)};
        ret->reflist_item_proxy() = {nullptr};
        delete curr;
        Iterator::list->_length--;
        disable();
        return ret;
    }

    template<PublicExtendsObject ElemT, typename ItemActionT>
    owned<ElemT> _M_ModifierT::replace_this(owned<ElemT> new_this)
    {
        ItemActionT i;
        if (Iterator::get() == new_this ||
            new_this == nullptr)
            return new_this;
        if (i.on_modifier_replace_preprocess(this, new_this) == false)
            return nullptr;
        RefT ret{std::move(Iterator::get())};
        Iterator::get() = std::move(new_this);
        i.on_modifier_replace(this, ret);
        return ret;
    }
#   undef  _M_ModifierT
} // namespace MTB

#endif
