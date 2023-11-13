#pragma once

#include <algorithm>
#include <any>
#include <array>
#include <chrono>
#include <climits>
#include <concepts>
#include <condition_variable>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <execution>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <future>
#include <numeric>
#include <optional>
#include <queue>
#include <random>
#include <ranges>
#include <semaphore>
#include <shared_mutex>
#include <source_location>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <string>
#include <syncstream>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>
#include <regex>
#include <bitset>
#include <cstdlib>
#include <stacktrace>

using namespace std::literals;

namespace nova
{
    namespace types
    {
        using u8  = uint8_t;
        using u16 = uint16_t;
        using u32 = uint32_t;
        using u64 = uint64_t;

        using i8  = int8_t;
        using i16 = int16_t;
        using i32 = int32_t;
        using i64 = int64_t;

        using uc8 = unsigned char;
        using c8  = char;
        using c16 = wchar_t;
        using c32 = char32_t;

        using f32 = float;
        using f64 = double;

        using b8 = std::byte;

        using usz = size_t;
    }

    using namespace types;

// -----------------------------------------------------------------------------

    template<typename... Ts>
    struct Overloads : Ts... {
        using Ts::operator()...;
    };

    template<typename... Ts> Overloads(Ts...) -> Overloads<Ts...>;

// -----------------------------------------------------------------------------

    template<typename T>
    T* Temp(T&& v)
    {
        return &v;
    }
}

// -----------------------------------------------------------------------------

#define NOVA_CONCAT_INTERNAL(a, b) a##b
#define NOVA_CONCAT(a, b) NOVA_CONCAT_INTERNAL(a, b)

// -----------------------------------------------------------------------------

#ifdef _WIN32
#  define NOVA_NO_INLINE __declspec(noinline)
#  define NOVA_FORCE_INLINE __forceinline
#endif

// -----------------------------------------------------------------------------

inline
void* __cdecl operator new[](size_t size, const char* /* name */, int /* flags */, unsigned /* debug_flags */, const char* /* file */, int /* line */)
{
	return ::operator new(size);
}

inline
void* __cdecl operator new[](size_t size, size_t align, size_t /* ??? */, const char* /* name */, int /* flags */, unsigned /* debug_flags */, const char* /* file */, int /* line */)
{
    return ::operator new(size, std::align_val_t(align));
}