#ifndef __OKAYLIB_DETAIL_IS_LVALUE_H__
#define __OKAYLIB_DETAIL_IS_LVALUE_H__

#include <type_traits>

namespace ok::detail {

template <class T> constexpr std::is_lvalue_reference<T&&> is_lvalue(T&&)
{
    return {};
}

} // namespace ok::detail

#endif
