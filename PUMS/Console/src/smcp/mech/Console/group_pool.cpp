/**
 * @file group_pool.cpp
 * @brief Явная инстанциация test::CGroup / CGroupBank.
 */

#include "smcp/mech/Console/group_pool.hpp"

template class smcp::test::CGroup<24>;
template class smcp::test::CGroupBank<24>;
template class smcp::test::CGroup<40>;
template class smcp::test::CGroupBank<40>;
