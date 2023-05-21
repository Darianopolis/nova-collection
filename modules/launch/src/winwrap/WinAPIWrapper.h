#pragma once
#ifndef WINUTIL_H
#define WINUTIL_H

#include <string>
#include <memory>
#include <vector>
#include <iostream>

/*
* -- Windows type aliases --
* When this header is loaded in a clean translation unit (without windows.h)
* we need to mock out all the windows types we might require. Unlike other
* windows type mocking headers we keep the types namespaced away here for tidy-ness.
*
* When loaded into a namespace WITH windows.h we just need to alias all the
* namespaced types to the underlying windows types. This allows the user to write
* using purely windows native types without any aliasing, and simply ensures that
* the compiler will reconcile the types with any clean headers
*/
#ifdef _WINDEF_

namespace win
{
    template <class T>
    class ComPtr
    {
        T* t;
    public:
        // Default
        ComPtr() noexcept
            : t(nullptr)
        {}

        ~ComPtr() noexcept
        {
            if (t)
                t->Release();
        }

        // Wrap
        ComPtr(T* t) noexcept
            : t(t)
        {}

        ComPtr& operator=(T* newT) noexcept
        {
            if (t == newT)
                return *this;

            if (t)
                t->Release();

            t = newT;
            return *this;
        }

        // Copy
        ComPtr(const ComPtr& p) noexcept
            : t(p.t)
        {
            t->AddRef();
        }

        ComPtr& operator=(const ComPtr& p) noexcept
        {
            if (t == p.t)
                return *this;

            if (t)
                t->Release();

            t = p.t;
            t->AddRef();

            return *this;
        }

        // Move
        ComPtr(ComPtr&& p) noexcept
            : t(p.t)
        {
            p.t = nullptr;
        }

        ComPtr& operator=(ComPtr&& p) noexcept
        {
            if (t == p.t)
                return *this;

            if (t)
                t->Release();

            t = p.t;
            p.t = nullptr;

            return *this;
        }

        // Query
        T* operator->()            const noexcept { return t; }
        auto operator<=>(T* other) const noexcept { return t <=> other; }
        bool exists()              const noexcept { return t != nullptr; }
        T& operator*()             const noexcept { return *t; }
        T* get()                   const noexcept { return t; }

        // Control
        void release() noexcept { if (t) { t->Release(); t = nullptr; } }
        T** set()      noexcept { release(); return &t; }

        T* extract() noexcept
        {
            T* _t = t;
            t = nullptr;
            return _t;
        }
    };

    // using IconPtr = std::unique_ptr<HICON, decltype([](HICON i) { if (i) DestroyIcon(i); })>;

    using LParam = LPARAM;
    using WParam = WPARAM;
    using HWnd = HWND;
    using HInstance = HINSTANCE;
    using LResult = LRESULT;
    using HResult = HRESULT;
    using DWord = DWORD;
    using UInt = UINT;
    using HIcon = HICON;
    using Guid = GUID;
    using TrackMouseEvent = TRACKMOUSEEVENT;
    using Rect = RECT;

    namespace gdi {
        using HPaintStruct = LPPAINTSTRUCT;
        using HDeviceContext = HDC;
        using HBrush = HBRUSH;
        using HBitmap = HBITMAP;
    }
}

#else

namespace win
{
    template <class T> class ComPtr { T* t; ComPtr(); ~ComPtr(); };
    class IconPtr { void* t; IconPtr(); ~IconPtr(); };

    using LParam = long long;
    using WParam = unsigned long long;
    using HWnd = void*;
    using HInstance = void*;
    using LResult = long long;
    using HResult = long;
    using DWord = unsigned long;
    using UInt = unsigned int;
    using HIcon = void*;
    struct Rect { long v[4]; };

    struct Guid { uint32_t b[4]; };
    struct TrackMouseEvent { HWnd b[24 / sizeof(HWnd)]; };

    namespace gdi
    {
        using HPaintStruct = void*;
        using HDeviceContext = void*;
        using HBrush = void*;
        using HBitmap = void*;
    }
}

#endif

#ifdef __d3d11_2_h__
namespace win::d3d11
{
    using Device = ID3D11Device;
}
#else
namespace win::d3d11
{
    struct Device;
}
#endif

#ifdef __dxgi1_3_h__
namespace win::dxgi
{
    using Device = IDXGIDevice;
    using Factory2 = IDXGIFactory2;
}
#else
namespace win::dxgi
{
    struct Device;
    struct Factory2;
}
#endif

// FIXME - This should pick up a header from DirectComposition!
#ifdef __dxgi1_3_h__
namespace win::dcomp
{
    using Device = IDCompositionDevice;
    using Visual = IDCompositionVisual;
    using Target = IDCompositionTarget;
}
#else
namespace win::dcomp
{
    struct Device;
    struct Visual;
    struct Target;
}
#endif

#ifdef _D2D1_H_
namespace win::d2d1
{
    using Factory = ID2D1Factory;
    // using Factory1 = ID2D1Factory1;
    // using DeviceContext = ID2D1DeviceContext;
    using DCRenderTarget = ID2D1DCRenderTarget;
    using SolidColorBrush = ID2D1SolidColorBrush;
    using Bitmap = ID2D1Bitmap;
}
#else
namespace win::d2d1
{
    struct Factory;
    struct DCRenderTarget;
    struct DeviceContext;
    struct SolidColorBrush;
    struct Bitmap;
}
#endif

#ifdef DWRITE_H_INCLUDED
namespace win::dwrite
{
    using Factory = IDWriteFactory;
    using TextFormat = IDWriteTextFormat;
    using TextLayout = IDWriteTextLayout;
}
#else
namespace win::dwrite
{
    struct Factory;
    struct TextFormat;
    struct TextLayout;
}
#endif

#ifdef __IWICImagingFactory_INTERFACE_DEFINED__
namespace win::wic
{
    using Factory = IWICImagingFactory;
}
#else
namespace win::wic
{
    struct Factory;
}
#endif

namespace win
{
    /**
     * @brief Drop-in replacement for manually managed char arrays and string for use
     * when interacting with Windows APIs
     *
     * Will use the correct char type based on whether UNICODE is defined, and will
     * convert to and from UTF8 strings for maximum portability.
     *
     * Initializing a win::String with anything except for UTF8 or UTF16 strings
     * is not permitted and may result in data loss and/or corruption.
     */
    class String
    {
        wchar_t* data;
        size_t _capacity;
        size_t _length;

        String(wchar_t* data, size_t capacity, size_t length) noexcept;

        bool rawset(const char* str, size_t len) noexcept;
    public:
        ~String();

        String(const String& copy) noexcept;
        String& operator=(const String& copy);

        String(String&& move) noexcept;
        String& operator=(String&& ref) noexcept;

        String(size_t capacity) noexcept;

        String(const char* str, size_t len, size_t buf_len) noexcept;
        String(std::string_view str, size_t buf_len) noexcept;
        String(std::string_view str) noexcept;

        void swap(String& other) noexcept;

        friend void swap(String& a, String& b) noexcept
        {
            a.swap(b);
        }

        /**
         * @brief Template constructor to allow construction from static strings constants
         *    without needing a strlen call.
         * E.g. String buffer = "Hello World";
         *
         * @tparam len
         *     Note - The templated length INCLUDES the null terminator, so we need to
         *     subtract to get the string length, unlike when constructing from a
         *     string_view
         */
        template<size_t len>
        String(const char (&str)[len]) noexcept
            : String(str, len - 1, len)
        {}

        // Modification
        void set(const char* str, size_t len, size_t buf_len) noexcept;
        void set(std::string_view str) noexcept;
        void clear() noexcept;

        // Accessors
        wchar_t* lpwstr() noexcept;
        const wchar_t* lpcwstr() const noexcept;
        operator wchar_t*() noexcept;
        operator const wchar_t*() const noexcept;

        size_t length() const noexcept;
        size_t capacity() const noexcept;

        std::string string() const noexcept;
        std::string string(size_t len) const noexcept;
    };
}

#endif // !WINUTIL_H