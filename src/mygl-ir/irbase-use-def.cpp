#include "base/mtb-compatibility.hxx"
#include "mygl-ir/irbase-use-def.hxx"
#include <algorithm>
#include <sstream>
#include <string>

namespace MYGL::IRBase {

using osb_t = std::ostringstream;
using namespace MTB::Compatibility;

static std::string type_get_string(const Type *ty)
{
    if (ty == nullptr)
        return "<null type>";
    return ty->toString();
}

/** @class Value::ValueTypeUnmatchException */
Value::ValueTypeUnmatchException::ValueTypeUnmatchException(Type *expected, Type *real,
                                  std::string_view info, SourceLocation location)
    : MTB::Exception{
        ErrorLevel::CRITICAL,
        osb_fmt("ValueTypeUnmatchException at line ", 
                location.line(), ": Expected type `", type_get_string(expected),
                "`, but got `", type_get_string(real),
                "` (info: ", info, ")"),
        location},
      expected(expected), real(real) {
}
/** end class Value::ValueTypeUnmatchException */

/** @class Value */
Value::Value(ValueTID type_id, Type *value_type, uint32_t id)
    : _type_id(type_id),
      _value_type(value_type),
      _id(id) {}

Value::~Value() = default;

std::string Value::get_name_or_id() const
{
    if (_name.empty())
        return std::to_string(_id);
    return _name;
}
std::string &Value::initNameAsId()
{
    if (_name.empty())
        _name = std::to_string(_id);
    return _name;
}
/** end class Value */

/** @class User */
User::User(ValueTID type_id, Type *value_type)
    : Value(type_id, value_type){}

User::~User() = default;

bool User::addUseAsUser(owned<Use> use)
{
    for (Use *i : _list_as_user) {
        if (i == use)
            return false;
    }
    Use *puse = use;
    _list_as_user.append(std::move(use));
    return true;
}
bool User::removeUseAsUser(Use *use)
{
    if (use->_user != this) return false;
    use->get_modifier().remove_this();
    return true;
}

Use *User::addValue(Use::UseeGetHandle get_usee, Use::UseeSetHandle set_usee)
{
    Use *ret = new Use(this, std::move(get_usee), std::move(set_usee));
    _list_as_user.append(ret);
    return ret;
}
Use *User::addUncheckedValue(Value *&value)
{
    Use *ret = new Use(this, value);
    _list_as_user.append(ret);
    return ret;
}
Use *User::addUncheckedValue(owned<Value> &value)
{
    Use *ret = new Use(this, value);
    _list_as_user.append(ret);
    return ret;
}
size_t User::replaceAllUsee(Value *pattern, Value *new_usee)
{
    size_t ret = 0;
    for (Use *i : _list_as_user) {
        if (pattern == i->get_usee()) {
            i->swapUseeOut(new_usee);
            ret++;
        }
    }
    return ret;
}
/** end class User */

/** @class Use */
Use::Use(User *user, UseeGetHandle get_usee, UseeSetHandle set_usee)
    : _user(std::move(user)),
      _get_handle(std::move(get_usee)),
      _set_handle(std::move(set_usee)),
      _proxy{nullptr} {
}
Use::Use(User *user, Value *&usee)
    : _user(user) {
    Value **pusee = &usee;
    _get_handle = [pusee](){ return *pusee; };
    _set_handle = [pusee](Value *value)-> SetResult {
        *pusee = value;
        return SetResult{false};
    };
}
Use::Use(User *user, owned<Value> &usee)
    : _user(user) {
    owned<Value> *pusee = &usee;
    _get_handle = [pusee]() { return pusee->get(); };
    _set_handle = [pusee](Value *value) mutable {
        *pusee = owned(value);
        return SetResult{false};
    };
}

Use::~Use() = default;

bool Use::remove_usee()
{
    owned<Use> wasted = nullptr;
    SetResult result = _set_handle(nullptr);
    if (result.use_dies) {
        wasted = get_modifier().remove_this();
        return false;
    }
    return false;
}

owned<Value> Use::swapUseeOut(Value *usee)
{
    owned<Value> orig = get_usee();
    if (orig == usee)
        return orig;
    orig->removeUseAsUsee(this);
    set_usee(usee);
    auto &usee_la_usee = usee->get_list_as_usee();

    if (std::find(usee_la_usee.cbegin(), usee_la_usee.cend(), this) == usee_la_usee.end())
        usee->addUseAsUsee(this);
    return orig;
}
/** end class Use */

} // namespace MYGL::IRBase
