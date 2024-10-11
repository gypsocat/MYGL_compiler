#ifndef __MTB_VFIELD_GETSET_H__
#define __MTB_VFIELD_GETSET_H__
#pragma once
#include "mtb-getter-setter.hxx"

#define DEFINE_VFIELD_GETTER(Type, property)\
    Type get_property() const { return get_shared_vfield()->property; }
#define DEFINE_VFIELD_REF_GETTER(Type, property)\
    Type const& get_property() const { return *get_shared_vfield()->property; }
#define DEFINE_VFIELD_REF_ACCESS(Type, property)\
    Type& property() const { return *get_shared_vfield()->property; }

#endif