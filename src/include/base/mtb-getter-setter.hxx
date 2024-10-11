#ifndef __MTB_GETTER_SETTER_H__
#define __MTB_GETTER_SETTER_H__

/* getter与setter默认宏 */
#define DECLARE_GETTER(property) auto get_##property() const;
#define DEFINE_GETTER(property) auto get_##property() const { return _##property; }

#define DECLARE_TYPED_GETTER(Type, property) Type get_##property() const;
#define DEFINE_TYPED_GETTER(Type, property) \
    Type get_##property() const {\
        return (Type)(_##property);\
    }
#define DECLARE_VIRTUAL_GETTER(Type, property)\
    virtual Type get_##property() const;
#define DEFINE_VIRTUAL_GETTER(Type, property) \
    virtual Type get_##property() const {\
        return (Type)(_##property);\
    }
#define DECLARE_TYPED_VIRTUAL_GETTER(Type, property)\
    virtual Type get_##property() const;
#define DEFINE_TYPED_VIRTUAL_GETTER(Type, property) \
    virtual Type get_##property() const {\
        return (Type)(_##property);\
    }
#define DECLARE_ABSTRACT_GETTER(Type, property)\
    virtual Type get_##property() const = 0;

/* bool类型的getter命名与常规不同 */
#define DECLARE_BOOL_GETTER(property) \
    bool property() const;
#define DEFINE_BOOL_GETTER(property) \
    bool property() const { return _##property; }
#define DECLARE_VIRTUAL_BOOL_GETTER(property) \
    virtual bool property() const;
#define DEFINE_VIRTUAL_BOOL_GETTER(property) \
    virtual bool property() const { return _##property; }
#define DECLARE_ABSTRACT_BOOL_GETTER(property) \
    virtual bool property() const = 0;
#define DEFINE_DEFAULT_TRUE_GETTER(property) \
    virtual bool property() const { return true; }
#define DEFINE_DEFAULT_FALSE_GETTER(property) \
    virtual bool property() const { return false; }
#define OVERRIDE_GET_TRUE(property) \
    bool property() const override { return true; }
#define OVERRIDE_GET_FALSE(property) \
    bool property() const override { return false; }

/* setters */
#define DECLARE_SETTER(Type, property)\
    void set_##property(Type property);
#define DEFINE_SETTER(Type,  property)\
    void set_##property(Type value) {\
        _##property = value;\
    }
#define DECLARE_COPY_SETTER(Type, property)\
    void set_##property(Type const &value);
#define DEFINE_COPY_SETTER(Type,  property)\
    void set_##property(Type const &value) {\
        _##property = value;\
    }
#define DECLARE_MOVE_SETTER(Type, property)\
    void set_##property(Type &&value);
#define DEFINE_MOVE_SETTER(Type,  property)\
    void set_##property(Type &&value) {\
        _##property = std::move(value);\
    }
#define DECLARE_VIRTUAL_SETTER(Type, property)\
    virtual void set_##property(Type property);
#define DEFINE_VIRTUAL_SETTER(Type,  property)\
    virtual void set_##property(Type value) {\
        _##property = value;\
    }
#define DECLARE_VIRTUAL_COPY_SETTER(Type, property)\
    virtual void set_##property(Type const &value);
#define DEFINE_VIRTUAL_COPY_SETTER(Type,  property)\
    virtual void set_##property(Type const &value) {\
        _##property = value;\
    }
#define DECLARE_VIRTUAL_MOVE_SETTER(Type, property)\
    virtual void set_##property(Type &&value);
#define DEFINE_VIRTUAL_MOVE_SETTER(Type,  property)\
    virtual void set_##property(Type &&value) {\
        _##property = std::move(value);\
    }
#define DECLARE_ABSTRACT_SETTER(Type, property)\
    virtual void set_##property(Type property) = 0;
#define DECLARE_ABSTRACT_COPY_SETTER(Type, property)\
    virtual void set_##property(Type const &value) = 0;
#define DECLARE_ABSTRACT_MOVE_SETTER(Type, property)\
    virtual void set_##property(Type &&value) = 0;
#define DECLARE_ACCESSOR(Type, property) Type &property();
#define DEFINE_ACCESSOR(Type, property)\
    Type &property() { return (Type&)(_##property); }

/* Getter/Setter捆绑宏 */
#define DECLARE_GETSET(Type, property)\
    DECLARE_TYPED_GETTER(Type, property)\
    DECLARE_SETTER(Type, property)
#define DEFINE_GETSET(Type, property)\
    DEFINE_TYPED_GETTER(Type, property)\
    DEFINE_SETTER(Type, property)
#define DECLARE_INLINE_GETSET(InType, OutType, property)\
    DECLARE_TYPED_GETTER(OutType, property)\
    DECLARE_COPY_SETTER(InType, property)\
    DECLARE_MOVE_SETTER(InType, property)
#define DEFINE_INLINE_GETSET(InType, OutType, property)\
    DEFINE_TYPED_GETTER(OutType, property)\
    DEFINE_COPY_SETTER(InType, property)\
    DEFINE_MOVE_SETTER(InType, property)
#define DEFINE_VIRTUAL_GETSET(Type, property)\
    DEFINE_TYPED_VIRTUAL_GETTER(Type, property)\
    DEFINE_VIRTUAL_SETTER(Type, property)
#define DECLARE_VIRTUAL_GETSET(Type, property)\
    DECLARE_TYPED_VIRTUAL_GETTER(Type, property)\
    DECLARE_VIRTUAL_SETTER(Type, property)
#define DECLARE_ABSTRACT_GETSET(Type, property)\
    DECLARE_ABSTRACT_GETTER(Type, property)\
    DECLARE_ABSTRACT_SETTER(Type, property)

/** Owned指针与其他指针的get/set又不一样。owned指针除了要接受来自普通指针的参数，
 *  还要接受来自右值引用的参数. */
#define DECLARE_UNRELATED_OWNED_GETSET(Type, property)\
    DECLARE_TYPED_GETTER(Type*, property)\
    DECLARE_SETTER(Type*, property)\
    DECLARE_MOVE_SETTER(owned<Type>, property)

#define DECLARE_OWNED_GETSET(Type, property)\
    DECLARE_TYPED_GETTER(Type*, property)\
    DECLARE_SETTER(Type*, property)\
    void set_##property(owned<Type> &&value) {\
        set_##property(value.get());\
    }

#define DECLARE_RREF_OWNED_GETSET(Type, property)\
    DECLARE_TYPED_GETTER(Type*, property)\
    DECLARE_MOVE_SETTER(owned<Type>, property)\
    void set_##property(Type* value) { set_##property(owned(value)); }

/* bool类型的Getter/Setter捆绑宏 */
#define DECLARE_BOOL_GETSET(property)\
    DECLARE_BOOL_GETTER(property)\
    DECLARE_SETTER(bool, property)
#define DECLARE_VIRTUAL_BOOL_GETSET(property)\
    DECLARE_VIRTUAL_BOOL_GETTER(property)\
    DECLARE_VIRTUAL_SETTER(bool, property)
#define DECLARE_ABSTRACT_BOOL_GETSET(property)\
    DECLARE_ABSTRACT_BOOL_GETTER(property)\
    DECLARE_ABSTRACT_SETTER(bool, property)
#define DEFINE_BOOL_GETSET(property)\
    DEFINE_BOOL_GETTER(property)\
    DEFINE_SETTER(bool, property)
#define DEFINE_VIRTUAL_BOOL_GETSET(property)\
    DEFINE_VIRTUAL_BOOL_GETTER(property)\
    DEFINE_VIRTUAL_SETTER(bool, property)

/** getter/setter绑定器 */
#define DEFINE_GETTER_WRAPPER(Type, property, target)\
    Type get_##property() const { return get_##target(); }
#define DEFINE_SETTER_WRAPPER(Type, property, target)\
    void set_##property(Type property) { set_##target(property); }
#define DEFINE_GETSET_WRAPPER(Type, property, target)\
    DEFINE_GETTER_WRAPPER(Type, property, target)\
    DEFINE_SETTER_WRAPPER(Type, property, target)
#define DEFINE_SYNONYMOUS_GETTER(property, target)\
    bool property() const { return target(); }
#define DEFINE_AUTONYMOUS_GETTER(property, target)\
    bool property() const { return !target(); }

#endif