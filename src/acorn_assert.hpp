#pragma once
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <string_view>

namespace acorn::detail
{
inline void assert_fail(const char* expr, const char* file, int line, const char* func,
                        const char* fmt = nullptr, ...) noexcept
{
    std::fprintf(stderr, "[ACORN_ASSERT] %s:%d in %s\n  failed: %s\n", file, line, func, expr);

    if (fmt)
    {
        std::fprintf(stderr, "  message: ");
        va_list args;
        va_start(args, fmt);
        std::vfprintf(stderr, fmt, args);
        va_end(args);
        std::fprintf(stderr, "\n");
    }

#if defined(_MSC_VER)
    __debugbreak();
#elif defined(__GNUC__) || defined(__clang__)
    __builtin_trap();
#else
    std::abort();
#endif
    std::abort();
}
}  // namespace acorn::detail

#ifndef NDEBUG
    #ifndef ACORN_ENABLE_ASSERTS
        #define ACORN_ENABLE_ASSERTS 1
    #endif
#endif

#if ACORN_ENABLE_ASSERTS
    #define ACORN_ASSERT(cond)                                                     \
        do                                                                         \
        {                                                                          \
            if (!(cond))                                                           \
            {                                                                      \
                ::acorn::detail::assert_fail(#cond, __FILE__, __LINE__, __func__); \
            }                                                                      \
        } while (0)

    #define ACORN_ASSERT_MSG(cond, ...)                                                         \
        do                                                                                      \
        {                                                                                       \
            if (!(cond))                                                                        \
            {                                                                                   \
                ::acorn::detail::assert_fail(#cond, __FILE__, __LINE__, __func__, __VA_ARGS__); \
            }                                                                                   \
        } while (0)

#endif