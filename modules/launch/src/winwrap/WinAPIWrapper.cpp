#include "WinAPIWrapper.h"

#include <iostream>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

std::string toUTF8String(const wchar_t* utf16, const int utf16_len) noexcept
{
    auto str_len = utf16_len * 3 + 1;
    auto str = std::vector<char>(str_len);
    auto bytes = WideCharToMultiByte(
        CP_UTF8,
        0,
        &utf16[0],
        utf16_len,
        str.data(),
        str_len,
        nullptr,
        nullptr
    );
    return std::string(str.data(), bytes);
}

namespace win {
    String::String(const String& copy) noexcept
        : _length(copy._length)
        , _capacity(copy._capacity)
        , data(new wchar_t[copy._capacity])
    {
        std::copy(&copy.data[0], &copy.data[0] + copy._capacity, &data[0]);
    }


    String& String::operator=(const String& copy)
    {
        auto copy_ = copy;
        copy_.swap(*this);
        return *this;
    }

    void String::swap(String& other) noexcept
    {
        std::swap(_length, other._length);
        std::swap(_capacity, other._capacity);
        std::swap(data, other.data);
    }

    String::String(String&& move) noexcept
        : _length(move.length())
        , _capacity(move.capacity())
        , data(move.data)
    {
        move.data = nullptr;
    }

    String& String::operator=(String&& ref) noexcept
    {
        delete[] data;

        _length = ref.length();
        _capacity = ref.capacity();
        data = ref.data;
        ref.data = nullptr;

        return *this;
    }

    /**
     * @brief Internal constructor
     *
     * @param data   Takes Ownership
     * @param capacity capacity > length
     * @param length   length <= sizeof(data)
     */
    String::String(wchar_t* data, size_t capacity, size_t length) noexcept
        : data(data)
        , _capacity(capacity)
        , _length(length)
    {}

    /**
     * @brief  Preallocated buffer constructor
     *
     * We need to explicitly null out the first byte of our buffer so that lpcwstr()
     * and lpwstr() return well-formed c-style strings
     *
     * @param capacity
     */
    String::String(size_t capacity) noexcept
        : String(new wchar_t[capacity], capacity, 0)
    {
        data[0] = 0;
    }

    String::~String() noexcept
    {
        delete[] data;
    }

    /**
     * @brief Directly set the contents of the buffer using a multibyte conversion.
     * If an error occurs during the transformation (E.g. an insufficiently sized
     * buffer), this function will reset the buffer to length 0 (although any written
     * data will not be cleared)
     *
     * Places a null terminator after the transformed string for C API compatibility.
     *
     * @param str
     * @param len
     * @return true if string was successfully transformed,
     *     otherwise false (Use GetLastError() to get actual error)
     */
    bool String::rawset(const char* str, size_t len) noexcept
    {
        if (len > 0)
        {
            _length = MultiByteToWideChar(CP_UTF8, MB_PRECOMPOSED,
                str, len, data, _capacity - 1);

            if (_length == 0) [[unlikely]]
            {
                data[_length] = 0;
                return false;
            }
        }
        else
        {
            _length = 0;
        }

        data[_length] = 0;
        return true;
    }

    String::String(const char* str, size_t len, size_t buf_len) noexcept
        : _capacity(buf_len)
        , data(new wchar_t[buf_len])
    {
        rawset(str, len);
    }

    String::String(std::string_view str, const size_t buf_len) noexcept
        : String(str.data(), str.length(), buf_len)
    {}

    String::String(std::string_view str) noexcept
        : String(str.data(), str.length(), str.length() + 1)
    {}

    void String::set(const char* str, size_t len, size_t buf_len) noexcept
    {
        if (buf_len > _capacity)
        {
            delete[] data;
            _capacity = std::max((size_t)(_capacity * 1.5), buf_len);
            data = new wchar_t[_capacity];
        }

        rawset(str, len);
    }

    void String::clear() noexcept
    {
        _length = 0;
    }

    void String::set(std::string_view str) noexcept
    {
        set(str.data(), str.length(), str.length() + 1);
    }

    wchar_t* String::lpwstr() noexcept
    {
        return data;
    }

    const wchar_t* String::lpcwstr() const noexcept
    {
        return data;
    }

    String::operator wchar_t*() noexcept
    {
        return data;
    }

    String::operator const wchar_t*() const noexcept
    {
        return data;
    }

    size_t String::length() const noexcept
    {
        return _length;
    }

    size_t String::capacity() const noexcept
    {
        return _capacity;
    }

    std::string String::string() const noexcept
    {
        return toUTF8String(data, _length);
    }

    std::string String::string(size_t len) const noexcept
    {
        return toUTF8String(data, _length);
    }
}