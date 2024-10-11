#ifndef __MYGL_IRBASE_VALUE_ID_MANAGER_H__
#define __MYGL_IRBASE_VALUE_ID_MANAGER_H__

#include "base/mtb-object.hxx"
#include "base/mtb-id-allocator.hxx"
#include "irbase-use-def.hxx"
#include <cstddef>
#include <unordered_map>

namespace MYGL::IRBase {

class ValueIDManager: public MTB::Object {
public:
    using ValueIDMapT = std::unordered_map<Value*, size_t>;
    using IDValueMapT = std::unordered_map<size_t, Value*>;
public:
    explicit ValueIDManager();
    explicit ValueIDManager(ValueIDManager const &mgr);
    explicit ValueIDManager(ValueIDManager &&mgr);

    ValueIDManager &operator=(ValueIDManager const &mgr);
    ValueIDManager &operator=(ValueIDManager &&mgr);

    size_t getOrRegisterValue(Value *value);
    bool   unregisterValue(Value *value);
private:
    IDAllocator _id_allocator;
    ValueIDMapT _value_id_map;
    IDValueMapT _id_value_map;
}; // class ValueIDManager

} // namespace MYGL::IRBase

#endif