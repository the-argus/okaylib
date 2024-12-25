#ifndef __OKAYLIB_TRAITS_IS_INSTANCE_H__
#define __OKAYLIB_TRAITS_IS_INSTANCE_H__

#include <type_traits>

namespace ok::detail {
// template specializations to detect whether a type is an instance of
template <class, template <typename> typename>
struct is_instance : public std::false_type
{};

template <class Y, template <typename> typename U>
struct is_instance<U<Y>, U> : public std::true_type
{};
} // namespace ok::detail

#endif
