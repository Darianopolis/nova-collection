#pragma once
#ifndef X64_TYPES_H
#define X64_TYPES_H

// Assert we're in a valid 64 bit environment
static_assert(sizeof(char) == 1);
static_assert(sizeof(wchar_t) == 2);
static_assert(sizeof(short) == 2);
static_assert(sizeof(int) == 4);
static_assert(sizeof(long long) == 8);
static_assert(sizeof(void*) == 8);
static_assert(sizeof(size_t) == 8);

// Unsigned types
using u8 = unsigned char;
using u16 = unsigned short;
using u32 = unsigned int;
using u64 = unsigned long long;

// Signed types
using i8 = char;
using i16 = short;
using i32 = int;
using i64 = long long;

// Character types
using c8 = char8_t;
using c16 = char16_t;
using c32 = char32_t;

// Float types
using f32 = float;
using f64 = double;

#endif // !X64_TYPES_H