#include "base/mtb-exception.hxx"
#include "mygl-ir/ir-basicblock.hxx"
#include "mygl-ir/ir-instruction.hxx"
#include "mygl-ir/irbase-value-visitor.hxx"

#define MYGL_GETELEMPTR_UNPACK_TYPE_VOID_OR_THROW(primary, after_unpack) \
    if ((after_unpack)->is_void_type()) { \
        throw TypeMismatchException { \
            (primary), \
            osb_fmt( \
                "Type for unpacking cannot be void* in progress ", \
                type_layer_top, " of ", indexes.size() \
            ), \
            CURRENT_SRCLOC_F \
        }; \
    }
#define MYGL_GETELEMPTR_UNPACK_TYPE_NON_INDEXABLE_THROW(before_unpack) \
    throw TypeMismatchException { \
        before_unpack, \
        osb_fmt( \
            "type ", before_unpack->toString(), \
            "should be indexable while the unpacking process is ", \
            type_layer_top, " of ", indexes.size()), \
        CURRENT_SRCLOC_F \
    };

namespace MYGL::IR {
    inline namespace getelemptr_impl {
        using TypeLayerStackT = GetElemPtrSSA::TypeLayerStackT;
        using ValueArrayT     = Value::ValueArrayT;
        using namespace std::string_literals;
        using namespace std::string_view_literals;
        using namespace MTB::Compatibility;

        static inline void
        check_type_index_integer_or_throw(ValueArrayT const &indexes,
                                          size_t index)
        {
            if (Type *index_value_type = indexes[index]->get_value_type();
                !index_value_type->is_integer_type()) {
                throw TypeMismatchException {
                    index_value_type,
                    ""s,
                    CURRENT_SRCLOC_F
                };
            }
        }

        static inline IntType*
        check_type_integer_or_throw(Type *value_type)
        {
            if (!value_type->is_integer_type()) {
                throw TypeMismatchException {
                    value_type,
                    ""s, CURRENT_SRCLOC_F
                };
            }
            return static_cast<IntType*>(value_type);
        }

        static inline PointerType*
        collection_get_type_or_throw(Value *collection)
        {
            if (collection == nullptr) {
                throw NullException {
                    "GetElemPtrSSA.collection",
                    ""s, CURRENT_SRCLOC_F
                };
            }
            Type *collection_type = collection->get_value_type();
            if (!collection_type->is_pointer_type()) {
                throw TypeMismatchException {
                    collection_type,
                    "Collection type should be pointer type"s,
                    CURRENT_SRCLOC_F
                };
            }
            return static_cast<PointerType*>(collection_type);
        }
        
#   if MYGL_HAS_STRUCT_TYPE == true
        static inline
        TypeLayerStackT make_type_layer_stack(PointerType *collection_type,
                                              ValueArrayT const&   indexes) {
            static_assert(false, "NOT IMPLEMENTED YET");
        }
#   else
        static inline TypeLayerStackT
        make_type_layer_stack(PointerType *collection_type,
                              ValueArrayT const&   indexes)
        {
            TypeLayerStackT ret_tlstack(indexes.size() + 1);
            size_t       type_layer_top = 0;
            ret_tlstack[0] = collection_type;

            while (type_layer_top < indexes.size()) {
                check_type_index_integer_or_throw(indexes, type_layer_top);
                Type *before_unpack = ret_tlstack[type_layer_top];
                Type *after_unpack  = nullptr;
                switch (before_unpack->get_type_id()) {
                case TypeTID::ARRAY_TYPE:
                    after_unpack = static_cast<ArrayType*>(before_unpack)->get_element_type();
                    break;
                case TypeTID::POINTER_TYPE:
                    after_unpack = static_cast<PointerType*>(before_unpack)->get_target_type();
                    MYGL_GETELEMPTR_UNPACK_TYPE_VOID_OR_THROW(collection_type, after_unpack);
                    break;
                default:
                    MYGL_GETELEMPTR_UNPACK_TYPE_NON_INDEXABLE_THROW(before_unpack);
                }
                type_layer_top++;
                ret_tlstack[type_layer_top] = after_unpack;
            }

            return ret_tlstack;
        }
#   endif

        struct index_vsetter {
            GetElemPtrSSA  *self;
            uint32_t index_order;

            Use::SetResult operator()(Value *index) {
                return index == nullptr ?
                       remove_this() : set_usee(index);
            }
        private:
            Use::SetResult set_usee(Value *index)
            {
                check_type_integer_or_throw(index->get_value_type());
                ValueUsePair &pairef = self->indexes()[index_order];
                owned<Value> &valref = pairef.value;
                if (valref != nullptr)
                    valref->removeUseAsUsee(pairef.use);
                if (index != nullptr)
                    index->addUseAsUsee(pairef.use);
                valref = index;
                return Use::SetResult{false};
            }

            Use::SetResult remove_this()
            {
                ValueUsePair &pairef = self->indexes()[index_order];
                owned<Value> &valref = pairef.value;
                if (valref != nullptr)
                    valref->removeUseAsUsee(pairef.use);
                valref = nullptr;
                return Use::SetResult{false};
            }
        };
    } // inline namespace getelemptr_impl

    GetElemPtrSSA::RefT GetElemPtrSSA::
    CreateFromPointer(owned<Value> ptr_collection, const ValueArrayT &indexes)
    {
        if (ptr_collection == nullptr) throw NullException {
            "GetElemPtrSSA::Create...()::ptr_collection"sv,
            {}, CURRENT_SRCLOC_F
        };
        Type *value_type = ptr_collection->get_value_type();
        if (!value_type->is_pointer_type()) throw TypeMismatchException {
            value_type,
            "GetElemPtrSSA operand type should be pointer type"s,
            CURRENT_SRCLOC_F
        };
        PointerType *collection_type = static_cast<PointerType*>(value_type);
        TypeLayerStackT tls = make_type_layer_stack(collection_type, indexes);
        Type        *elemty = tls.back();
        TypeContext *etyctx = elemty->get_type_context();
        PointerType *ret_ty = etyctx->getPointerType(elemty);

        RETURN_MTB_OWN(GetElemPtrSSA, ret_ty, std::move(ptr_collection), indexes);
    }
/* ================ [public class GetElemPtrSSA] ================ */
    GetElemPtrSSA::GetElemPtrSSA(PointerType *value_type, owned<Value> collection, ValueArrayT const &indexes)
        : Instruction(ValueTID::GET_ELEM_PTR_SSA, value_type, OpCode::GET_ELEMENT_PTR),
          _layer_stack(0), _collection(std::move(collection)),
          _indexes(0) {

        /* 检查 `value_type` 是否为空 */
        if (value_type == nullptr) {
            throw NullException {
                "GetElemPtrSSA.value_type"sv,
                ""s, CURRENT_SRCLOC_F
            };
        }
        /* 检查 `collection` 是不是指针 */
        PointerType *collection_type = collection_get_type_or_throw(_collection);
        _layer_stack = make_type_layer_stack(collection_type, indexes);

        /* 检查索引展开栈的结果类型. 要求该类型是目标指针类型的元素. */
        if (Type *lty = _layer_stack.back(),
                 *rty = value_type->get_target_type();
            lty != rty && !lty->equals(rty)) {
            throw TypeMismatchException {
                _layer_stack.back(),
                "Value pointer type should equal to"
                "the GetElemPtr result type"s,
                CURRENT_SRCLOC_F
            };
        }

        /* 现在插入 `collection` 的值 */
        MYGL_ADD_USEE_PROPERTY(collection);

        /* 现在插入 `indexes` 的值 */
        _indexes.resize(indexes.size());
        for (uint32_t i = 0; i < _indexes.size(); i++) {
            _indexes[i].value = indexes[i];
            _indexes[i].use = addValue(
                [this, i]() { return get_indexes()[i].value.get(); },
                index_vsetter{this, i});
        }
    }

    void GetElemPtrSSA::set_collection(owned<Value> collection)
    {
        if (_collection == collection)
            return;
        Use  *collection_use = list_as_user().at(0);
        Type *type_require = _layer_stack[0];

        if (_collection != nullptr)
            _collection->removeUseAsUsee(collection_use);

        if (collection != nullptr) {
            Type *new_type = collection->get_value_type();
            if (type_require != new_type && !type_require->equals(new_type)) {
                throw TypeMismatchException {
                    new_type,
                    "new collection type should be equal to"
                    "the previous collection type"s, CURRENT_SRCLOC_F
                };
            }
            collection->addUseAsUsee(collection_use);
        }
        _collection = std::move(collection);
    }

    Type *GetElemPtrSSA::get_collection_type() const {
        return _layer_stack[0]->get_base_type();
    }

    Value const* GetElemPtrSSA::getIndex(size_t index_order) const {
        return _indexes.at(index_order).value;
    }
    Value *GetElemPtrSSA::getIndex(size_t index_order) {
        return _indexes.at(index_order).value;
    }
    bool GetElemPtrSSA::setIndex(size_t index_order, owned<Value> index)
    {
        if (index_order >= _indexes.size())
            return false;
        index_vsetter{this, uint32_t(index_order)}(index);
        return true;
    }


/* ================ [GetElemPtrSSA::override parent] ================ */
    void GetElemPtrSSA::accept(IValueVisitor &visitor) {
        visitor.visit(this);
    }
    Value *GetElemPtrSSA::operator[](size_t index)
    {
        if (index == 0)
            return _collection;
        return _indexes.at(index - 1);
    }
    void GetElemPtrSSA::traverseOperands(ValueAccessFunc fn)
    {
        if (fn(*_collection)) return;
        for (ValueUsePair &vp: _indexes) {
            if (fn(*vp.value))
                return;
        }
    }
    size_t GetElemPtrSSA::get_operand_nmemb() const
    {
        return 1 + _indexes.size();
    }

/* ================ [GetElemPtrSSA::override signal] ================ */

    void GetElemPtrSSA::on_parent_plug(BasicBlock *parent)
    {
        Instruction::on_parent_plug(parent);
        _connect_status = Instruction::ConnectStatus::CONNECTED;
    }
    void GetElemPtrSSA::on_parent_finalize()
    {
        set_collection(nullptr);
        for (auto &[value, use]: _indexes) {
            if (value == nullptr)
                continue;
            value->removeUseAsUsee(use);
            value.reset();
        }
        _connect_status = Instruction::ConnectStatus::FINALIZED;
    }
    void GetElemPtrSSA::on_function_finalize()
    {
        _collection.reset();
        for (auto &[value, use]: _indexes)
            value.reset();
        _connect_status = Instruction::ConnectStatus::FINALIZED;
    }
} // namespace MYGL::IR