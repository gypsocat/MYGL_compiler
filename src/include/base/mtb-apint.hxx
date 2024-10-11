#include <algorithm>
#include <cassert>
#include <cstdint>

#ifndef __MTB_INLINE__
#ifdef __GNUC__
#define __MTB_INLINE__ __attribute__((__always_inline__))
#else
#define __MTB__INLINE__ constexpr inline
#endif
#endif


namespace MTB {

struct APInt {
    enum CompareResult: uint8_t {
        FALSE = 0,
        LT = 0b0001, EQ = 0b0010, GT = 0b0100,
        GE = 0b0110, NE = 0b0101, LE = 0b0011
    };
public:
    APInt(): _binary_bits(sizeof(intptr_t) * 8), _instance(0){}
    APInt(uint8_t binary_bits, int64_t value)
        : _binary_bits(binary_bits) {
        assert(binary_bits <= 64);
        set_value(value);
    }
    APInt(APInt const &another)
        : _binary_bits(another._binary_bits),
          _instance(another._instance){}

    uint8_t  get_binary_bits()  const { return _binary_bits; }
    int64_t  get_signed_value() const {
        uint64_t ret = (sign_neg()? 0xFFFF'FFFF'FFFF'FFFF: 0) | _instance; 
        return reinterpret_cast<int64_t&>(ret);
    }
    uint64_t get_unsigned_value() const { return _instance; }
    void set_value(int64_t value) noexcept {
        _instance = value & _get_binary_pmask();
    }
    bool sign_neg() const  { return (_instance >> (_binary_bits - 1)) != 0; }
    bool has_error() const { return (_binary_bits > 64); }

    APInt &operator=(APInt const &rhs) {
        _binary_bits = rhs._binary_bits;
        _instance    = rhs._instance;
        return *this;
    }
    APInt &operator=(int64_t rhs) { set_value(rhs); return *this; }
    operator bool() const { return _instance != 0; }

    /** 相等函数: 强相等 */
    bool equals(APInt const &rhs) const {
        return _binary_bits == rhs._binary_bits &&
               _instance    == rhs._instance;
    }
    /** 相等函数: 弱相等 */
    bool signed_equals(APInt const &rhs) const {
        return get_signed_value() == rhs.get_signed_value();
    }
    bool unsigned_equals(APInt const &rhs) const {
        return get_unsigned_value() == rhs.get_unsigned_value();
    }

    APInt neg()  const;
    APInt lnot() const;
    APInt add(APInt rhs)  const;
    APInt sub(APInt rhs)  const;
    APInt mul(APInt rhs)  const;
    APInt sdiv(APInt rhs) const;
    APInt udiv(APInt rhs) const;
    APInt srem(APInt rhs) const;
    APInt urem(APInt rhs) const;
    APInt land(APInt rhs) const;
    APInt lor(APInt rhs)  const;
    APInt lxor(APInt rhs) const;
    APInt lshl(uint8_t rhs) const;
    APInt ashl(uint8_t rhs) const;
    APInt shr(uint8_t rhs)  const;
    APInt rshl(uint8_t rhs) const;
    APInt rshr(uint8_t rhs) const;
    APInt zext(uint8_t binary_bits) const;
    APInt sext(uint8_t binary_bits) const;
    APInt trunc(uint8_t binary_bits) const;

    CompareResult scmp(APInt rhs) const {
        int64_t lv  = _instance, rv = rhs._instance;
        uint8_t ret = CompareResult::FALSE;
        if (lv == rv)
            ret |= CompareResult::EQ;
        if (lv > rv)
            ret |= CompareResult::GT;
        if (lv < rv)
            ret |= CompareResult::LT;
        return CompareResult{ret};
    }

    __MTB_INLINE__ APInt operator+(APInt const &rhs) const { return add(rhs); }
    __MTB_INLINE__ APInt operator-(APInt const &rhs) const { return sub(rhs); }
    __MTB_INLINE__ APInt operator-() const { return neg(); }
    __MTB_INLINE__ APInt operator*(APInt const &rhs) const { return mul(rhs); }
    __MTB_INLINE__ APInt operator/(APInt const &rhs) const { return sdiv(rhs); }
    __MTB_INLINE__ APInt operator%(APInt const &rhs) const { return srem(rhs); }
    __MTB_INLINE__ APInt operator&(APInt const &rhs) const { return land(rhs); }
    __MTB_INLINE__ APInt operator|(APInt const &rhs) const { return lor(rhs); }
    __MTB_INLINE__ APInt operator^(APInt const &rhs) const { return lxor(rhs); }
    __MTB_INLINE__ APInt operator~() const { return lnot(); }
    __MTB_INLINE__ APInt operator!() const { return lnot(); }
    __MTB_INLINE__ APInt &operator+=(APInt const &rhs) { *this = add(rhs); return *this; }
    __MTB_INLINE__ APInt &operator-=(APInt const &rhs) { *this = sub(rhs); return *this; }
    __MTB_INLINE__ APInt &operator*=(APInt const &rhs) { *this = mul(rhs); return *this; }
    __MTB_INLINE__ APInt &operator/=(APInt const &rhs) { *this = sdiv(rhs); return *this; }
    __MTB_INLINE__ APInt &operator%=(APInt const &rhs) { *this = srem(rhs); return *this; }
    __MTB_INLINE__ APInt &operator&=(APInt const &rhs) { *this = land(rhs); return *this; }
    __MTB_INLINE__ APInt &operator|=(APInt const &rhs) { *this = lor(rhs); return *this; }
    __MTB_INLINE__ APInt &operator^=(APInt const &rhs) { *this = lxor(rhs); return *this; }

    __MTB_INLINE__ APInt operator+(int64_t rhs) const { return add(APInt{_binary_bits, rhs}); }
    __MTB_INLINE__ APInt operator-(int64_t rhs) const { return sub(APInt{_binary_bits, rhs}); }
    __MTB_INLINE__ APInt operator*(int64_t rhs) const { return mul(APInt{_binary_bits, rhs}); }
    __MTB_INLINE__ APInt operator/(int64_t rhs) const { return sdiv(APInt{_binary_bits, rhs}); }
    __MTB_INLINE__ APInt operator%(int64_t rhs) const { return srem(APInt{_binary_bits, rhs}); }
    __MTB_INLINE__ APInt operator&(int64_t rhs) const { return land(APInt{_binary_bits, rhs}); }
    __MTB_INLINE__ APInt operator|(int64_t rhs) const { return lor(APInt{_binary_bits, rhs}); }
    __MTB_INLINE__ APInt operator^(int64_t rhs) const { return lxor(APInt{_binary_bits, rhs}); }
    __MTB_INLINE__ APInt &operator+=(int64_t rhs) { *this = add(APInt{_binary_bits, rhs}); return *this; }
    __MTB_INLINE__ APInt &operator-=(int64_t rhs) { *this = sub(APInt{_binary_bits, rhs}); return *this; }
    __MTB_INLINE__ APInt &operator*=(int64_t rhs) { *this = mul(APInt{_binary_bits, rhs}); return *this; }
    __MTB_INLINE__ APInt &operator/=(int64_t rhs) { *this = sdiv(APInt{_binary_bits, rhs}); return *this; }
    __MTB_INLINE__ APInt &operator%=(int64_t rhs) { *this = srem(APInt{_binary_bits, rhs}); return *this; }
    __MTB_INLINE__ APInt &operator&=(int64_t rhs) { *this = land(APInt{_binary_bits, rhs}); return *this; }
    __MTB_INLINE__ APInt &operator|=(int64_t rhs) { *this = lor(APInt{_binary_bits, rhs}); return *this; }
    __MTB_INLINE__ APInt &operator^=(int64_t rhs) { *this = lxor(APInt{_binary_bits, rhs}); return *this; }
private:
    uint64_t _instance;
    uint8_t  _binary_bits;

    uint64_t _get_binary_pmask() const {
        return (uint64_t(0x1) << _binary_bits) - 1;
    }
    uint64_t _get_binary_nmask() const {
        return ~_get_binary_pmask();
    }
    uint64_t _get_sign_mask() const {
        return (uint64_t(0x1) << (_binary_bits - 1));
    }
}; // struct TruncableInt

/** impl */
inline APInt APInt::neg() const
{
    APInt ret;
    ret._binary_bits = _binary_bits;
    ret.set_value(-get_signed_value());
    return ret;
}
inline APInt APInt::lnot() const
{
    APInt ret;
    ret._binary_bits = _binary_bits;
    ret._instance = (!_instance) & _get_binary_pmask();
    return ret;
}

inline APInt APInt::add(APInt rhs) const
{
    APInt ret;
    ret._binary_bits = std::max(_binary_bits, rhs._binary_bits);
    ret.set_value(get_unsigned_value() + rhs.get_unsigned_value());
    return ret;
}
inline APInt APInt::sub(APInt rhs) const
{
    APInt ret;
    ret._binary_bits = std::max(_binary_bits, rhs._binary_bits);
    ret.set_value(get_signed_value() - rhs.get_signed_value());
    return ret;
}
inline APInt APInt::mul(APInt rhs) const
{
    APInt ret;
    ret._binary_bits = std::max(_binary_bits, rhs._binary_bits);
    ret.set_value(get_signed_value() * rhs.get_signed_value());
    return ret;
}
inline APInt APInt::sdiv(APInt rhs) const
{
    APInt ret;
    ret._binary_bits = std::max(_binary_bits, rhs._binary_bits);
    ret.set_value(get_signed_value() / rhs.get_signed_value());
    return ret;
}
inline APInt APInt::udiv(APInt rhs) const
{
    APInt ret;
    ret._binary_bits = std::max(_binary_bits, rhs._binary_bits);
    ret.set_value(get_unsigned_value() / rhs.get_unsigned_value());
    return ret;
}
inline APInt APInt::srem(APInt rhs) const
{
    APInt ret;
    ret._binary_bits = std::max(_binary_bits, rhs._binary_bits);
    ret.set_value(get_signed_value() % rhs.get_signed_value());
    return ret;
}
inline APInt APInt::urem(APInt rhs) const
{
    APInt ret;
    ret._binary_bits = std::max(_binary_bits, rhs._binary_bits);
    ret.set_value(get_unsigned_value() % rhs.get_unsigned_value());
    return ret;
}
inline APInt APInt::land(APInt rhs) const
{
    APInt ret;
    ret._binary_bits = std::max(_binary_bits, rhs._binary_bits);
    ret._instance = get_unsigned_value() & rhs.get_unsigned_value();
    return ret;
}
inline APInt APInt::lor(APInt rhs) const
{
    APInt ret;
    ret._binary_bits = std::max(_binary_bits, rhs._binary_bits);
    ret._instance = get_unsigned_value() | rhs.get_unsigned_value();
    return ret;
}
inline APInt APInt::lxor(APInt rhs) const
{
    APInt ret;
    ret._binary_bits = std::max(_binary_bits, rhs._binary_bits);
    ret._instance = get_unsigned_value() ^ rhs.get_unsigned_value();
    return ret;
}
inline APInt APInt::zext(uint8_t binary_bits) const
{
    if (binary_bits < _binary_bits) {
        APInt error_ret;
        error_ret._binary_bits = 0xFF;
        return error_ret;
    }
    APInt ret;
    ret._binary_bits = binary_bits;
    ret._instance    = _instance;
    return ret;
}
inline APInt APInt::sext(uint8_t binary_bits) const
{
    if (binary_bits < _binary_bits) {
        APInt error_ret;
        error_ret._binary_bits = 0xFF;
        return error_ret;
    }
    APInt ret;
    ret._binary_bits = binary_bits;
    ret._instance = (sign_neg() ? _get_binary_pmask() : 0) & _instance;
    return ret;
}
inline APInt APInt::trunc(uint8_t binary_bits) const
{
    if (binary_bits > _binary_bits) {
        APInt error_ret;
        error_ret._binary_bits = 0xFF;
        return error_ret;
    }
    APInt ret;
    ret._binary_bits = binary_bits;
    ret._instance = _get_binary_pmask() & _instance;
    return ret;
}

/** end impl */

} // namespace MTB


#ifdef __MTB_INLINE__
#undef __MTB_INLINE__
#endif