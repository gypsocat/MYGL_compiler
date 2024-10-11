#pragma once
#ifndef __MTB_OWN_MACRO_H__
#define __MTB_OWN_MACRO_H__
#include "mtb-object.hxx"
#include <cstdio>

#define MTB_OWN(ret, ObjType, ...) \
    do {\
        ObjType             *__mtb_ret_rawp__;\
        std::allocator<ObjType> __allocator__;\
        try {\
            __mtb_ret_rawp__ = __allocator__.allocate(1);\
            new(__mtb_ret_rawp__) ObjType(__VA_ARGS__);\
        } catch (std::bad_alloc &ba) {\
            fputs(ba.what(), stderr), fputc('\n', stderr);\
            std::terminate();\
        } catch (std::exception &e) {\
            __allocator__.deallocate(__mtb_ret_rawp__, 1);\
            throw;\
        }\
        ret = MTB::owned(__mtb_ret_rawp__);\
    } while (false)

#define MTB_NEW_OWN(ObjType, object_name, ...) \
    MTB::owned<ObjType> object_name;\
    MTB_OWN(object_name, ObjType, __VA_ARGS__);

#define RETURN_MTB_OWN(ObjType, ...) do {\
        ObjType             *__mtb_ret_rawp__;\
        std::allocator<ObjType> __allocator__;\
        try {\
            __mtb_ret_rawp__ = __allocator__.allocate(1);\
            new(__mtb_ret_rawp__) ObjType(__VA_ARGS__);\
        } catch (std::bad_alloc &ba) {\
            fputs(ba.what(), stderr), fputc('\n', stderr);\
            std::terminate();\
        } catch (std::exception &e) {\
            __allocator__.deallocate(__mtb_ret_rawp__, 1);\
            throw;\
        }\
        return MTB::owned(__mtb_ret_rawp__);\
    } while (false)

#define MTB_OWN_NOEXCEPT(ObjType, ...) MTB::owned<ObjType>{new ObjType{__VA_ARGS__}}

#endif