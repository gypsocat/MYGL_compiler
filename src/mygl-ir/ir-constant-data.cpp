#include "base/mtb-exception.hxx"
#include "mygl-ir/ir-constant.hxx"
#include "mygl-ir/irbase-value-visitor.hxx"
#include <cmath>
#include <stdexcept>

namespace MYGL::IR {

using MTB::owned;
using oss = std::ostringstream;

/** @class Constant */
Constant::Constant(ValueTID type_id, Type *value_type)
    : User(type_id, value_type) {
}
Constant::ConstantKind Constant::get_const_kind() const
{
    uint8_t ret = 0;
    ret |= is_integer() ? uint8_t(ConstantKind::INTEGER): 0;
    ret |= is_float()   ? uint8_t(ConstantKind::FLOAT)  : 0;
    ret |= is_array()   ? uint8_t(ConstantKind::ARRAY)  : 0;
    if (is_pointer()) {
        if (is_function())             ret |= uint8_t(ConstantKind::FUNCTION);
        else if (is_global_variable()) ret |= uint8_t(ConstantKind::GLOBAL);
        else                           ret |= uint8_t(ConstantKind::POINTER);
    }
    return ConstantKind(ret);
}
/* static class Constant */
owned<Constant> Constant::CreateZeroOrUndefined(Type *type)
{
    if (type == nullptr)
        throw NullException("value_type", "Value type connot be null");
    if (type->is_integer_type())
        return new IntConst(static_cast<IntType*>(type), 0);
    if (type->is_float_type())
        return new FloatConst(static_cast<FloatType*>(type), 0.0);
    if (type->is_array_type())
        return ArrayExpr::CreateEmpty(static_cast<ArrayType*>(type));
    return new UndefinedConst(type);
}
/** end class Constant */

/** @class ConstantData */
ConstantData::ConstantData(ValueTID type_id, Type *value_type)
    : Constant(type_id, value_type){}

bool ConstantData::equals(const Constant *rhs) const
{
    const ConstantData *cd = dynamic_cast<const ConstantData*>(rhs);
    if (cd == nullptr)
        return false;
    CompareResult result = cd->compare(owned<ConstantData>{
                            const_cast<ConstantData*>(cd)});
    return (result == CompareResult::EQ) ||
           (result == CompareResult::TRUE);
}
/* static class ConstantData */
owned<ConstantData> ConstantData::CreateZero(Type *value_type) noexcept(false)
{
    if (value_type == nullptr) {
        throw NullException{
            "value_type", "Value type connot be null",
            CURRENT_SRCLOC
        };
    }
    if (value_type->is_integer_type())
        return new IntConst(static_cast<IntType*>(value_type), 0);
    if (value_type->is_float_type())
        return new FloatConst(static_cast<FloatType*>(value_type), 0.0);
    throw TypeMismatchException {
            value_type,
            (oss("type ") << value_type << " is not derived from `ValueType`").str(),
            CURRENT_SRCLOC
        };
}
/** end class ConstantData */

/** @class IntConst */
IntConst::IntConst(IntType *value_type, int64_t value)
    : ConstantData(ValueTID::INT_CONST, value_type),
      _value(value_type->get_binary_bits(), value) {
    _name = std::to_string(_value.get_signed_value());
}
IntConst::IntConst(IntType *value_type, APInt value, bool as_signed)
    : ConstantData(ValueTID::INT_CONST, value_type),
      _value(value_type->get_binary_bits(), 0) {
    _value.set_value(as_signed ?
                     value.get_signed_value():
                     value.get_unsigned_value());
    _name = std::to_string(_value.get_signed_value());
}
IntConst::~IntConst() = default;

owned<ConstantData> IntConst::castToClosest(Type *target) const
{
    IntType *value_ty = static_cast<IntType*>(get_value_type());
    if (target->is_integer_type()) {
        IntType *ity = static_cast<IntType*>(target);
        uint8_t target_bits = ity->get_binary_bits();
        if (target_bits > value_ty->get_binary_bits())
            return new IntConst(ity, get_value().sext(target_bits));
        return this;
    }
    if (target->is_float_type()) {
        FloatType *fty = static_cast<FloatType*>(target);
        return new FloatConst(fty, get_float_value());
    }
    throw TypeMismatchException {
        target,
        "target type doesn't meet condition `is_value_type() == true`",
        CURRENT_SRCLOC_F
    };
}
owned<ConstantData> IntConst::add(const owned<ConstantData> rhs) const
{
    IntType *ity = static_cast<IntType*>(get_value_type());
    Type    *rty = rhs->get_value_type();

    if (rty->is_integer_type()) {
        IntType *rity = static_cast<IntType*>(rty);
        IntType *ret_ty = ity->get_binary_bits() >= rity->get_binary_bits() ?
                          ity: rity;
        return new IntConst {
            ret_ty,
            get_value() + rhs.static_get<IntConst>()->get_value()
        };
    }
    if (rty->is_float_type()) {
        return new FloatConst {
            static_cast<FloatType*>(rty),
            get_float_value() + rhs->get_float_value()
        };
    }
    return rhs->add(castToClosest(rty));
}
owned<ConstantData> IntConst::sub(const owned<ConstantData> rhs) const {
    return add(rhs->neg());
}
owned<ConstantData> IntConst::mul(const owned<ConstantData> rhs) const
{
    IntType *ity = static_cast<IntType*>(get_value_type());
    Type    *rty = rhs->get_value_type();
    if (rhs->is_integer()) {
        IntType *irty = static_cast<IntType*>(rty);
        IntType *ret_ty = ity->get_binary_bits() >= irty->get_binary_bits() ?
                          ity: irty;
        return new IntConst {
            ret_ty,
            get_value() * rhs.static_get<IntConst>()->get_value()
        };
    }
    return rhs->mul(castToClosest(rty));
}
owned<ConstantData> IntConst::sdiv(const owned<ConstantData> rhs) const
{
    if (rhs->is_zero()) {
        throw std::runtime_error("Cannot be divided by zero");
    }
    IntType *ity = static_cast<IntType*>(get_value_type());
    Type    *rty = rhs->get_value_type();
    if (rhs->is_integer()) {
        IntType *irty = static_cast<IntType*>(rty);
        IntType *ret_ty = ity->get_binary_bits() >= irty->get_binary_bits() ?
                          ity: irty;
        return new IntConst {
            ret_ty,
            get_value() / rhs.static_get<IntConst>()->get_value()
        };
    }
    return castToClosest(rty)->sdiv(rhs);
}
owned<ConstantData> IntConst::udiv(const owned<ConstantData> rhs) const
{
    IntType *ity = static_cast<IntType*>(get_value_type());
    Type    *rty = rhs->get_value_type();
    if (rhs->is_integer()) {
        IntType *irty = static_cast<IntType*>(rty);
        IntType *ret_ty = ity->get_binary_bits() >= irty->get_binary_bits() ?
                          ity: irty;
        return new IntConst {
            ret_ty,
            get_value().udiv(rhs.static_get<IntConst>()->get_value())
        };
    }
    return castToClosest(rty)->udiv(rhs);
}
owned<ConstantData> IntConst::smod(const owned<ConstantData> rhs) const
{
    if (rhs->is_integer()) {
        return new IntConst {
            static_cast<IntType*>(rhs->get_value_type()),
            get_value().srem({
                get_value().get_binary_bits(),
                rhs->get_int_value()
            })};
    }
    return castToClosest(rhs->get_value_type())->smod(rhs);
}
owned<ConstantData> IntConst::umod(const owned<ConstantData> rhs) const
{
    if (rhs->is_integer()) {
        return new IntConst {
            static_cast<IntType*>(rhs->get_value_type()),
            get_value().srem({
                get_value().get_binary_bits(),
                int64_t(rhs->get_uint_value())
            })};
    }
    return castToClosest(rhs->get_value_type())->umod(rhs);
}

void IntConst::accept(IValueVisitor &visitor) { visitor.visit(this); }
CompareResult IntConst::compare(const owned<ConstantData> rhs) const
{
    if (rhs->is_integer()) {
        CompareResult res = CompareResult::FALSE;
        if (get_int_value() > rhs->get_int_value())
            res = CompareResult(size_t(res) | size_t(CompareResult::GT));
        if (get_int_value() < rhs->get_int_value())
            res = CompareResult(size_t(res) | size_t(CompareResult::LT));
        if (get_int_value() == rhs->get_int_value())
            res = CompareResult(size_t(res) | size_t(CompareResult::EQ));
        return res;
    }
    if (rhs->is_float()) {
        CompareResult res = CompareResult::FALSE;
        if (get_float_value() > rhs->get_float_value())
            res = CompareResult(size_t(res) | size_t(CompareResult::GT));
        if (get_float_value() < rhs->get_float_value())
            res = CompareResult(size_t(res) | size_t(CompareResult::LT));
        if (get_float_value() == rhs->get_float_value())
            res = CompareResult(size_t(res) | size_t(CompareResult::EQ));
        return res;
    }
    return CompareResult::FALSE;
}
/** static class IntConst */
static InlineInit<IntConst> _BooleanTrue(IntType::i1, 1),
                            _BooleanFalse(IntType::i1, 0);
owned<IntConst> IntConst::BooleanTrue(&_BooleanTrue.instance),
                IntConst::BooleanFalse(&_BooleanFalse.instance);
/** end class IntConst */

/** @class FloatConst */
namespace float_const_impl {

}
owned<FloatConst> FloatConst::Create(FloatType *value_type, double value)
{
    RETURN_MTB_OWN(FloatConst, value_type, value);
}

FloatConst::FloatConst(FloatType *value_type, double value) noexcept
    : ConstantData(ValueTID::FLOAT_CONST, value_type),
      _value(value) {
}

FloatConst::~FloatConst() = default;

void FloatConst::accept(IValueVisitor &visitor) {
    visitor.visit(this);
}

owned<ConstantData> FloatConst::castToClosest(Type *target_type) const
{
    if (target_type->is_integer_type() ||
        target_type->get_binary_bits() < get_value_type()->get_binary_bits())
        return this;
    if (target_type->is_float_type() &&
        target_type->get_binary_bits() > get_value_type()->get_binary_bits())
        return Create(static_cast<FloatType*>(target_type), _value);
    throw TypeMismatchException {
        target_type,
        "Target type must be float type",
        CURRENT_SRCLOC_F
    };
}

owned<ConstantData> FloatConst::add(const owned<ConstantData> rhs) const
{
    FloatType *fty = static_cast<FloatType*>(get_value_type());
    Type      *rty = rhs->get_value_type();
    if (rhs->is_float()) {
        FloatType *frty   = static_cast<FloatType*>(rty);
        FloatType *ret_ty = fty->get_binary_bits() >= frty->get_binary_bits() ?
                            fty: frty;
        return Create(
            ret_ty, get_float_value() + rhs->get_float_value()
        );
    }
    if (rhs->is_integer()) {
        return new FloatConst{
            fty, get_float_value() + rhs->get_float_value()
        };
    }
    return rhs->castToClosest(fty)->add(castToClosest(rty));
}
owned<ConstantData> FloatConst::sub(const owned<ConstantData> rhs) const {
    return add(rhs->neg());
}
owned<ConstantData> FloatConst::mul(const owned<ConstantData> rhs) const
{
    FloatType *fty = static_cast<FloatType*>(get_value_type());
    Type      *rty = rhs->get_value_type();
    size_t lhs_bits = fty->get_binary_bits();
    size_t rhs_bits = fty->get_binary_bits();
    double ret = get_value() * rhs->get_float_value();
    if (rhs->is_float() && lhs_bits < rhs_bits)
        fty = static_cast<FloatType*>(rty);
    return Create(fty, ret);
}
owned<ConstantData> FloatConst::sdiv(const owned<ConstantData> rhs) const
{
    FloatType *fty = static_cast<FloatType*>(get_value_type());
    Type      *rty = rhs->get_value_type();
    size_t lhs_bits = fty->get_binary_bits();
    size_t rhs_bits = fty->get_binary_bits();
    double ret = get_value() / rhs->get_float_value();
    if (rhs->is_float() && lhs_bits < rhs_bits)
        fty = static_cast<FloatType*>(rty);
    return Create(fty, ret);
}
owned<ConstantData> FloatConst::smod(const owned<ConstantData> rhs) const
{
    FloatType *fty = static_cast<FloatType*>(get_value_type());
    Type      *rty = rhs->get_value_type();
    size_t lhs_bits = fty->get_binary_bits();
    size_t rhs_bits = fty->get_binary_bits();
    double ret = std::fmod(get_value(), rhs->get_float_value());
    if (rhs->is_float() && lhs_bits < rhs_bits)
        fty = static_cast<FloatType*>(rty);
    return Create(fty, ret);
}
CompareResult FloatConst::compare(const owned<ConstantData> rhs) const
{
    CompareResult res = CompareResult::FALSE;
    if (get_float_value() > rhs->get_float_value())
        res = CompareResult(size_t(res) | size_t(CompareResult::GT));
    if (get_float_value() < rhs->get_float_value())
        res = CompareResult(size_t(res) | size_t(CompareResult::LT));
    if (get_float_value() == rhs->get_float_value())
        res = CompareResult(size_t(res) | size_t(CompareResult::EQ));
    return res;
}
bool FloatConst::equals(const Constant *rhs) const
{
    if (rhs->is_integer() || rhs->is_float())
        return get_float_value() == static_cast<ConstantData const*>(rhs)->get_float_value();
    return false;
}
/** end class FloatConst */

/** @class ZeroDataConst */
ZeroDataConst::ZeroDataConst(TypeContext &ctx)
    : ConstantData(ValueTID::ZERO_CONST, ctx.getIntType(0)),
      IConstantZero(this) {
}

owned<ConstantData> ZeroDataConst::castToClosest(Type *target) const {
    return CreateZero(target);
}
owned<Constant> ZeroDataConst::make_nonzero_instance() const {
    return CreateZero(get_value_type());
}

void ZeroDataConst::accept(IValueVisitor &visitor) {
    return visitor.visit(this);
}
CompareResult ZeroDataConst::compare(const owned<ConstantData> rhs) const
{
    if (rhs->is_integer()) {
        IntConst *ival = rhs.static_get<IntConst>();
        CompareResult res = CompareResult::FALSE;
        if (0 >  ival->get_int_value())
            res = CompareResult(size_t(res) | size_t(CompareResult::GT));
        if (0 <  ival->get_int_value())
            res = CompareResult(size_t(res) | size_t(CompareResult::LT));
        if (0 == ival->get_int_value())
            res = CompareResult(size_t(res) | size_t(CompareResult::EQ));
        return res;
    }
    if (rhs->is_float()) {
        CompareResult res = CompareResult::FALSE;
        FloatConst *fval = rhs.static_get<FloatConst>();
        if (0.0 >  fval->get_float_value())
            res = CompareResult(size_t(res) | size_t(CompareResult::GT));
        if (0.0 <  fval->get_float_value())
            res = CompareResult(size_t(res) | size_t(CompareResult::LT));
        if (0.0 == fval->get_float_value())
            res = CompareResult(size_t(res) | size_t(CompareResult::EQ));
        return res;
    }
    return CompareResult::FALSE;
}
/** end class ZeroDataConst */

/** @class UndefinedConst */
void UndefinedConst::accept(IValueVisitor &visitor) {
    visitor.visit(this);
}
/** end class UndefinedConst */

} // namespace MYGL::IR