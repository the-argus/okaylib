#ifndef __OKAYLIB_DETAIL_OPT_DECL_H__
#define __OKAYLIB_DETAIL_OPT_DECL_H__

namespace ok {
struct nullopt_t
{};

inline constexpr nullopt_t nullopt{};

template <typename payload_t, typename = void> class opt;
} // namespace ok

#endif
