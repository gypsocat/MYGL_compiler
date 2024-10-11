#pragma once
#if __cplusplus <= 202002L
#include "../mtb-object.hxx"
#include "../mtb-exception.hxx"
#include <cstddef>
#include <functional>
#include <sstream>
#include <utility>

namespace MTB {

template<PublicExtendsObject ElemT, typename ActionPredT>
struct RefListProxy;

template<PublicExtendsObject ElemT>
interface RefListActions;

/** @class RefList
 * @brief 为MTB::Object的子类设计的组合式双向链表, 经过合理设计的元素可以根据自身的地址
 *        快速生成一个指向自己的迭代器，用于在自身周围做增删查改等“微操手术”。
 *
 * @param ElemT 元素类型. 约束: 是Object的子类，且必须有`iterator`, `prev`与`next`
 *        这三个只读属性.
 *        要写一个符合条件的ElemT, 可以放一个`RefListProxy`类型的成员帮你实现这几个属性。
 * @param ActionPredT 迭代访问检查器。约束: 要求拥有`RefListActions`模板接口的所有方法，
 *        `Iterator`在进行“微操手术”时需要调用这些方法做检查。
 *        自定义一个`ActionPredT`接口最快的方法是继承`RefListActions<ElemT>`. */
template<PublicExtendsObject ElemT, typename ActionPredT = RefListActions<ElemT>>
class RefList: public MTB::Object {
public:
    struct Iterator;

    /* STL兼容 */
    using value_type      = ElemT;
    using reference       = ElemT&;
    using const_reference = ElemT const&;
    using pointer         = ElemT*;
    using const_pointer   = ElemT const*;
    using size_type       = size_t;
    using difference_type = size_t;
    using iterator        = Iterator;
    using const_iterator  = const Iterator;
    using os_t = std::ostringstream;

    using ElemRefT   = owned<ElemT>;
    using AccessFunc = std::function<bool(ElemT &)>;

    friend ElemT;
    friend RefListProxy<ElemT, ActionPredT>;
    friend RefList<ElemT> CopyRefList(RefList<ElemT> const& another);
    friend struct Iterator;
    friend interface RefListActions<ElemT>;
protected:
    /** @struct Node
     * @brief 链表结点. 默认对外不可见, 需要Iterator帮它做好封装 */
    struct Node {
        Node *prev, *next;
        ElemRefT instance;
    }; // struct Node
public:
    /** @struct Iterator
     * @brief 顺序读写迭代器, 可以在自己周围进行增删查改等微操手术。
     *
     * 由于链表的特殊性, RefList没有随机访问迭代器。 */
    struct Iterator {
        friend interface RefListActions<ElemT>;
        Node    *node;
        RefList *list;

        ElemRefT &operator*() { return node->instance; }
        ElemRefT *operator->() { return &get(); }
        ElemRefT &get() { return node->instance; }
        ElemRefT const& operator*() const { return node->instance; }
        ElemRefT const& get() const { return node->instance; }
        bool operator==(Iterator const &it) const { return node == it.node && list == it.list; }
        bool operator!=(Iterator const &it) const { return node != it.node || list != it.list; }
        bool begins() const {
            return list->front() == node->instance;
        }
        bool ends() const {
            return list->back() == node->instance;
        }

        Iterator &operator++() {
            if (node != nullptr)
                node = node->next;
            return *this;
        }
        Iterator operator++(int) {
            Iterator ret{*this};
            operator++();
            return ret;
        }
        Iterator &operator--() {
            if (node != nullptr)
                node = node->prev;
            return *this;
        }
        Iterator operator--(int) {
            Iterator ret{*this};
            operator--();
            return ret;
        }
        Iterator get_prev() const {
            if (node == nullptr)
                return {nullptr, list};
            return {node->prev, list};
        }
        Iterator get_next() const {
            if (node == nullptr)
                return {nullptr, list};
            return {node->next, list};
        }

        bool append(ElemRefT elem)
        {
            ActionPredT a;
            if (a.onIteratorAppend(*this) == false)
                return false;
            Node *self = node;
            Node *next = self->next;
            Node *new_node = new Node{self, next, std::move(elem)};
            self->next = new_node;
            next->prev = new_node;
            list->_length++;
            return true;
        }
        bool prepend(ElemRefT elem)
        {
            if (ActionPredT().onIteratorPrepend(*this) == false)
                return false;
            Node *self = node;
            Node *prev = self->prev;
            Node *new_node = new Node{prev, self, std::move(elem)};
            self->prev = new_node;
            prev->next = new_node;
            list->_length++;
            return true;
        }
        /** @fn remove_this
         * @brief 从链表中移除自己. 返回指向实例的智能指针. 假定你不使用返回值,
         *        那node成员就会被删除. 自己被移除以后, 指向自身的iterator会
         *        失效. */
        ElemRefT remove_this()
        {
            if (node == list->_node_end ||
                node == list->_node_begin||
                node == nullptr)
                return nullptr;
            if (ActionPredT().onIteratorDisable(*this) == false)
                return nullptr;

            Node *prev = node->prev;
            Node *next = node->next;
            prev->next = next;
            next->prev = prev;
            ElemRefT ret{std::move(node->instance)};
            delete node;
            *this = {nullptr, nullptr};
            return ret;
        }
        /** @fn remove_next()
         * @brief 移除自己后面的那一个, 返回指向实例的智能指针. */
        ElemRefT remove_next()
        {
            Iterator next = {node->next, list};
            if (next != nullptr && next.node->next != nullptr)
                return nullptr;
            if (ActionPredT().onIteratorRemoveNext(*this) == false)
                return nullptr;
            return std::move(next.remove_this());
        }
        /** @fn remove_next()
         * @brief 移除自己前面的那一个, 返回指向实例的智能指针. */
        ElemRefT remove_prev()
        {
            Iterator prev = {node->prev, list};
            if (prev != nullptr && prev.node->prev != nullptr)
                return nullptr;
            if (ActionPredT().onIteratorRemovePrev(*this) == false)
                return nullptr;
            return std::move(prev.remove_this());
        }
        bool swap_next()
        {
            if (ends()) return false;
            Node *next = node->next;

            next->prev = node->prev;
            node->next = next->next;
            next->next = node;
            node->prev = next;
            return true;
        }
        bool swap_prev()
        {
            if (begins()) return false;
            Node *prev = node->prev;

            prev->next = node->next;
            node->prev = prev->prev;
            prev->prev = node;
            node->next = prev;
        }
        /** @fn replace_this(owned Elem another)
         * @brief 把自己指向的元素替换成另一个元素。 */
        ElemRefT replace_this(ElemRefT another)
        {
            if (ActionPredT().onIteratorReplace(*this, another) == false)
                return false;
            ElemRefT ret = std::move(node->instance);
            node->instance = another;
            return ret;
        }
    }; // struct Iterator

    struct RIterator: public Iterator {
        using Iterator::Iterator;

        RIterator &operator++()   { return --(*this); }
        RIterator operator++(int) { return (*this)--; }
        RIterator &operator--()   { return ++(*this); }
        RIterator operator--(int) { return (*this)++; }
        RIterator get_prev() { return RItetator(Iterator::get_next()); }
        RIterator get_next() { return RItetator(Iterator::get_prev()); }
        ElemRefT remove_next() { return Iterator::remove_prev(); }
        ElemRefT remove_prev() { return Iterator::remove_next(); }
        bool swap_next() { return Iterator::swap_prev(); }
        bool swap_prev() { return Iterator::swap_next(); }
    }; // struct RIterator
public:
    explicit RefList() {
        _node_begin = new Node{nullptr, nullptr, nullptr};
        _node_end   = new Node{nullptr, nullptr, nullptr};
        _node_begin->next = _node_end;
        _node_end->prev   = _node_begin;
    }

    /** @fn RefList(RefList const& another) = delete
     * @brief 引用共享的拷贝构造函数是被禁用了
     *
     * 由于这张表的大多数容器类型都没有拷贝构造方法, 所以你不能复制构造.
     * 如果需要的话, 请使用owned指针共享, 或者使用MTB::CopyRefList()函数. */
    explicit RefList(RefList const& another) = delete;

    /** @fn RefList(RefList &&another)
     * @brief 移动构造，适用于任何情况。 */
    explicit RefList(RefList &&another)
        : RefList() {
        std::swap(_node_begin->next, another._node_begin->next);
        std::swap(_node_end->prev,   another._node_end->prev);
        std::swap(_length, another._length);
    }
    ~RefList() {
        clear();
        delete _node_begin;
        delete _node_end;
    }

    /* 属性 */

    /** @property length{get;} */
    size_type get_length() const { return _length; }
    size_type size() const { return _length; }
    bool empty() const { return _length == 0; }

    /* 遍历 */
    Iterator begin() { return {_node_begin->next, this}; }
    Iterator end()   { return {_node_end, this}; }

    RIterator rbegin() { return {_node_end->prev, this}; }
    RIterator rend()   { return {_node_begin,     this}; }
    const Iterator begin() const { return {_node_begin->next, this}; }
    const Iterator end()   const { return {_node_end, this}; }

    /* 元素索引 */
    Iterator iterator_at(size_t index)
    {
        if (empty()) {
            throw EmptySetException("RefList.iterator_at()",
                "you get none of iterators in an empty list");
        }
        if (index >= get_length()) {
            throw std::out_of_range(
                (os_t("The list has ")
                    << size()
                    << " elements while you want to get element "
                    << index).str());
        }
        Node *i = _node_begin->next;
        while (index >= 0)
            i = i->next;
        return {i, this};
    }
    ElemRefT &at(size_t index) {
        Iterator it = iterator_at(index);
        Node  *node = it.node;
        return node->instance;
    }

    ElemRefT &front() {
        if (empty()) {
            throw EmptySetException("RefList.front()",
                "cannot get first item from an empty list");
        }
        return *nodeof_front()->instance;
    }
    ElemRefT &back() {
        if (empty()) {
            throw EmptySetException("RefList.back()",
                "cannot get last item from an empty list");
        }
        return nodeof_back()->instance;
    }
    ElemRefT const& front() const {
        if (empty()) {
            throw EmptySetException("RefList",
                "cannot get first item from an empty list");
        }
        return *nodeof_front()->instance;
    }
    ElemRefT const& back() const {
        if (empty()) {
            throw EmptySetException("RefList",
                "cannot get last item from an empty list");
        }
        return *nodeof_back()->instance;
    }

    /* 增删 */
    void append(owned<ElemT> element)
    {
        Node *back      = nodeof_back();
        Node *new_node  = new Node{back, _node_end, std::move(element)};
        back->next      = new_node;
        _node_end->prev = new_node;
        _length++;
    }
    void prepend(owned<ElemT> element)
    {
        Node *front    = nodeof_front();
        Node *new_node = new Node{_node_begin, front, std::move(element)};
        front->prev    = new_node;
        _node_begin->next = new_node;
        _length++;
    }
    bool pop_front()
    {
        if (empty())
            return false;
        Node *front     = nodeof_front();
        Node *new_front = front->next;
        new_front->prev = _node_begin;
        _node_begin->next = new_front;
        delete front;
        _length--;
        return true;
    }
    bool pop_back()
    {
        if (empty())
            return false;
        Node *back     = nodeof_back();
        Node *new_back = back->prev;
        new_back->next  = _node_end;
        _node_end->prev = new_back;
        delete back;
        _length--;
        return true;
    }
    void remove(pointer element) {
        return element->get_iterator().remove_this();
    }
    void remove_if(AccessFunc pred)
    {
        Node *i = _node_begin->next;
        while (i != _node_end) {
            if (pred(*i)) {
                Node *removed = i;
                i = i->next;
                Iterator{removed, this}.remove_this();
            } else {
                i = i->next;
            }
        }
    }
    void clear()
    {
        Node *i = nodeof_front();
        while (i != _node_end) {
            Node *prev = i;
            i = i->next;
            delete prev;
        }
    }
    void clear(AccessFunc fn)
    {
        Node *i = nodeof_front();
        while (i != _node_end) {
            Node *prev = i;
            i = i->next;
            fn(*prev->instance);
            delete prev;
        }
    }
private:
    Node  *_node_begin, *_node_end;
    size_t _length;

    Node *nodeof_front() const { return _node_begin->next; }
    Node *nodeof_back()  const { return _node_end->next; }
}; // class RefList


template<PublicExtendsObject ElemT, typename ActionPredT = RefListActions<ElemT>>
struct RefListProxy {
    using NodeT = typename RefList<ElemT, ActionPredT>::Node;
public:
    /** @property node_next{get;access;} */
    NodeT  const& get_node_next() const { return *self_node->next; }
    NodeT  &node_next() { return *self_node->next; }
    NodeT *&node_next_ptr() { return self_node->next; }

    /** @property node_prev{get;access;} */
    NodeT  const& get_node_prev() const { return *self_node->prev; }
    NodeT  &node_prev() { return *self_node->prev; }
    NodeT *&node_prev_ptr() { return *self_node->prev; }

    /** @property next{get;access;} */
    ElemT const* get_next() const { return get_node_next()->instance; }
    ElemT *get_next() { return get_node_next()->instance; }
    owned<ElemT> &next() const { return get_node_next()->instance; }

    /** @property prev{get;access;} */
    ElemT const* get_prev() const { return get_node_next()->instance; }
    ElemT *get_prev() { return get_node_next()->instance; }
    owned<ElemT> &prev() const { return get_node_next()->instance; }

    NodeT *self_node;
}; // struct RefListProxy

/** @interface RefListActions
 * @brief RefList插入时需要满足的接口 */
template<PublicExtendsObject ElemT>
interface RefListActions {
    template<typename ActionT = RefListActions<ElemT>>
    using ParentT   = RefList<ElemT, ActionT>;
    template<typename ActionT = RefListActions<ElemT>>
    using IteratorT = typename ParentT<ActionT>::Iterator;

    /** @fn onIteratorAppend<ActionT>
     * @brief 信号接收器，当Iterator执行方法`append()`时触发.
     * @return bool 确定所有检查完毕后, 是否要继续执行`append()`方法.
     *  @retval true 执行 */
    template<typename ActionT = RefListActions<ElemT>>
    inline bool onIteratorAppend(IteratorT<ActionT> &it, ElemT *elem) const {
        return (elem != nullptr) &&
               (it.node != it.list->_node_end);
    }

    /** @fn onIteratorPrepend<ActionT>
     * @brief 信号接收器，当Iterator执行方法`prepend()`时触发.
     * @return bool 确定所有检查完毕后, 是否要继续执行`prepend()`方法.
     *  @retval true 执行 */
    template<typename ActionT = RefListActions<ElemT>>
    inline bool onIteratorPrepend(IteratorT<ActionT> &it, ElemT *elem) const {
        return (elem != nullptr) &&
               (it.node != it.list->_node_begin);
    }

    /** @fn onIteratorReplace<ActionT>
     * @brief 信号接收器，当Iterator执行方法`replace_this()`时触发.
     * @return bool 确定所有检查完毕后, 是否要继续执行`replace_this()`方法.
     *  @retval true 执行 */
    template<typename ActionT = RefListActions<ElemT>>
    inline bool onIteratorReplace(IteratorT<ActionT> &it, ElemT *elem) const {
        return true;
    }

    /** @fn onIteratorRemovePrev<ActionT>
     * @brief 信号接收器，当Iterator执行方法`remove_prev()`时触发.
     * @return bool 确定所有检查完毕后, 是否要继续执行`remove_prev()`方法.
     *  @retval true 执行 */
    template<typename ActionT = RefListActions<ElemT>>
    inline bool onIteratorRemovePrev(IteratorT<ActionT> &it) const noexcept {
        return true;
    }

    /** @fn onIteratorRemoveNext<ActionT>
     * @brief 信号接收器，当Iterator执行方法`remove_next()`时触发.
     * @return bool 确定所有检查完毕后, 是否要继续执行`remove_next()`方法.
     *  @retval true 执行 */
    template<typename ActionT = RefListActions<ElemT>>
    inline bool onIteratorRemoveNext(IteratorT<ActionT> &it) const noexcept {
        return true;
    }
}; // interface RefListActions

template<typename ElemT>
RefList<ElemT> CopyRefList(RefList<ElemT> const &another)
{
    using Node = typename RefList<ElemT>::Node;
    RefList<ElemT> ret;
    Node *cur = ret._node_begin;
    for (Node *i = another._node_begin->next;
            i != another._node_end;
            i = i->next) {
        Node *new_node = new Node {
            cur, ret._node_end,
            new ElemT(*i->instance)
        };
        cur->next = new_node;
        ret._node_end->prev = new_node;
    }
}

} // namespace MTB
#endif
