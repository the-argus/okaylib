#ifndef __OKAYLIB_DEFAILT_TEMPLATE_UTIL_REF_AS_CONST_H__
#define __OKAYLIB_DEFAILT_TEMPLATE_UTIL_REF_AS_CONST_H__

#include "okay/detail/type_traits.h"

namespace ok::detail
{
template <stdc::is_reference_c T>
struct ref_as_const
{
    private:
    static constexpr bool is_rvalue =
        stdc::is_rvalue_reference_v<T>;
    using noref =
        stdc::add_const_t<stdc::remove_reference_t<T>>;

    public:

    using type = stdc::conditional_t<is_rvalue, noref&&,
                            noref&>;
};

template <stdc::is_reference_c T>
using ref_as_const_t = typename ref_as_const<T>::type;
}

#endif
