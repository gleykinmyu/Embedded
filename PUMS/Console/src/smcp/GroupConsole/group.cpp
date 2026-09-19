/**
 * @file group.cpp
 * @brief Group: wire-запись GRUP.
 */

#include "smcp/GroupConsole/group.hpp"

#include <cstring>

namespace smcp {

void Group::clear() noexcept
{
    mech = Selection{};
    flag = {};
    std::memset(reserved_1, 0, sizeof(reserved_1));
    std::memset(name, 0, sizeof(name));
    std::memset(reserved_2, 0, sizeof(reserved_2));
}

void Group::setName(const char* group_name) noexcept
{
    if (group_name == nullptr) {
        name[0] = '\0';
        return;
    }
    std::strncpy(name, group_name, kGroupNameSize - 1u);
    name[kGroupNameSize - 1u] = '\0';
}

void Group::setBlocked(bool on) noexcept
{
    if (on) {
        flag.set(Flag::Blocked);
    } else {
        flag.clear(Flag::Blocked);
    }
}

} // namespace smcp
