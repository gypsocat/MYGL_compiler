#include "base/mtb-exception.hxx"
#include "mygl-ir/ir-basic-value.hxx"
#include "mygl-ir/irbase-value-visitor.hxx"
#include <stdexcept>
#include <string_view>

namespace MYGL::IR {

using namespace std::string_view_literals;

/** @class Argument */
Argument::Argument(Type *value_type, std::string_view name, Function *parent)
    : Value(ValueTID::ARGUMENT, value_type),
      _parent(parent) {
    if (!name.empty())
        set_name(name);
}
void Argument::accept(IValueVisitor &visitor) {
    visitor.visit(this);
}
/** end class Argument */

/** @class Mutable */
/* ================ [static class Mutable] ================ */
Mutable::RefT Mutable::
Create(Type *reg_type, Function *parent, int32_t mutable_index) noexcept(false)
{
    std::string_view pointer_name{};
    if (reg_type == nullptr)
        pointer_name = "Mutable::Create...()::reg_type"sv;
    if (parent == nullptr)
        pointer_name = "Mutable::Create...()::parent<Function>"sv;
    if (!pointer_name.empty()) throw NullException {
        pointer_name, {}, CURRENT_SRCLOC_F
    };
    if (mutable_index < 0) throw std::out_of_range(
        "Mutable register index shouldn't be negative. What happened "
        "with your error code?"
    );
    RETURN_MTB_OWN(Mutable, reg_type, parent, mutable_index);
}
/* ================ [public class Mutable] ================ */
Mutable::Mutable(Type *reg_type, Function *parent, int32_t mutable_index)
    : Value(ValueTID::MUTABLE, reg_type, mutable_index),
      _parent(parent), _index(mutable_index) {}

Mutable::~Mutable() = default;

void Mutable::accept(IValueVisitor &visitor) {}

} // namespace MYGL::IR