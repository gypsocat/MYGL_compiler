#ifndef __MTB_BASE_H__
#define __MTB_BASE_H__

/* 一些没有任何实际效果，只有说明作用的宏 */
#define interface struct
#define delegate  struct /* 闭包函数, 有oprerator()方法、存储状态的结构体 */
#define imcomplete /* 抽象类, 但是拼错了 */
#define incomplete /* 抽象类 */
#define abstract virtual

/* C++23 的栈回溯、强制编译期执行, 方便开发使用 */
#if __cplusplus >= 202302L
#   define MTB_CONSTINIT constinit
#   define MTB_CONSTEVAL consteval
#else
#   define MTB_CONSTINIT constexpr
#   define MTB_CONSTEVAL constexpr
#endif

/* C++20 的 likely/unlikely */
#if   __cplusplus >= 202002L
#   define MTB_LIKELY_IF(expr)    if (expr) [[likely]]
#   define MTB_UNLIKELY_IF(expr)  if (expr) [[unlikely]]
#   define MTB_LIKELY_ELSE        else [[likely]]
#   define MTB_UNLIKELY_ELSE      else [[unlikely]]
#   define MTB_REQUIRE(expr)      requires expr
#elif defined (__GNUC__)
#   define MTB_LIKELY_IF(expr)    if (__builtin_expect(bool(expr), 1))
#   define MTB_UNLIKELY_IF(expr)  if (__builtin_expect(bool(expr), 0))
#   define MTB_LIKELY_ELSE        else
#   define MTB_UNLIKELY_ELSE      else
#   define MTB_REQUIRE(expr)
#else
#   define MTB_LIKELY_IF          if
#   define MTB_UNLIKELY_IF        if
#   define MTB_LIKELY_ELSE        else
#   define MTB_UNLIKELY_ELSE      else
#   define MTB_REQUIRE(expr)
#endif

#define MTB_DEPRECATED(message) [[deprecated(message)]]

#define __MTB_DEBUG__     1
/* 你需要开启如下选项才能启用 __MTB_DEBUG_EXP__:
 * - GCC & Clang: `-lstdc++exp` */
#define __MTB_DEBUG_EXP__ 0

/* GNU 专有的 printf_format、nonnull, 方便开发检查使用 */
#ifdef __GNUC__
#   include <sys/cdefs.h>
#   define always_inline __always_inline
#   define mtb_noinline  __attribute__((noinline))
#   define MTB_PRINTF_FORMAT(_FormatStrIndex, _FormatArgIndex) \
        [[gnu::format(printf, _FormatStrIndex, _FormatArgIndex)]]
#   if __MTB_DEBUG__ != 0
#       define MTB_NONNULL(...)   [[gnu::nonnull(__VA_ARGS__)]]
#   else
#       define MTB_NONNULL(...)
#   endif
#elif defined(_MSC_VER)
#   define always_inline __forceinline
#   define mtb_noinline  __declspec(noinline)
#   define MTB_NONNULL(...)
#   define MTB_PRINTF_FORMAT(_FormatStrIndex, _FormatArgIndex)
#else
#   define always_inline inline
#   define mtb_noinline
#   define MTB_NONNULL(...)
#   define MTB_PRINTF_FORMAT(_FormatStrIndex, _FormatArgIndex)
#endif

#if __MTB_DEBUG__ != 0
#define debug_print(...) fprintf(stderr, __VA_ARGS__)
#else
#define debug_print(...)
#endif

#endif