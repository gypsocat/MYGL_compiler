#ifndef __MYGL_LANG_BASE_H__
#define __MYGL_LANG_BASE_H__ 1UL

#include "base/mtb-object.hxx"

#ifndef interface
/** 用于说明的宏，没有实际作用。 */
// 接口说明符: 用于接口继承，父类一定要是虚继承的，成员只能有虚函数。
#define interface struct
// 抽象函数说明符：就是纯虚函数，加了abstract的虚函数要么写"=0",要么留空函数体。
#define abstract virtual
// 抽象类说明符：主要是abstract被占用了，才不得不用imcomplete.
#define imcomplete
#endif

using cstring = const char*;
using ccstrptr = const char*;
using cmutstr = char*;
using cmstrptr = char*;
using myglptr = void*;

#endif