#ifndef __MTB_VFIELD_OBJECT_H__
#define __MTB_VFIELD_OBJECT_H__

/** @file mtb-vfield-object.hxx
 * @brief 支持共享字段的基类. */

#pragma once
#include "mtb-object.hxx"

namespace MTB {
    class VFieldObject: public Object {
    public:
        struct SharedField {
        }; // struct SharedField
    protected:
        SharedField const& _shared_vfield;
        template<typename VFieldT>
        VFieldT const& get_shared_field() const {
            return static_cast<VFieldT const&>(_shared_vfield);
        }

        VFieldObject(SharedField const& field)
            : _shared_vfield(field) {}
    }; // class VFieldObject
}; // namespace MTB

#endif