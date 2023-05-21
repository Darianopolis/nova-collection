#pragma once

#include <type_traits>
#include <mutex>

#include "Win32Include.hpp"

namespace overlay
{
    // https://docs.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes

    enum class KeyCode
    {
        MouseLButton = 0x01, MouseRButton, MouseCancel, MouseMButton, MouseXButton1,
        MouseXButton2,

        KeyBackspace = 0x08, KeyTab,
        KeyClear = 0x0C, KeyReturn,
        KeyShift = 0x10, KeyControl, KeyAlt, KeyPause, KeyCapsLock,
        KeyIME_Kana_Hanguel_Hangul, KeyIME_On, KeyIME_Junja, KeyIME_Final,
        KeyIME_Hanja_Kanji, KeyIME_Off,
        KeyEscape = 0x1B,
        KeyIME_Convert, KeyIME_NonConvert, KeyIME_Accept, KeyIME_ModeChange,
        KeySpace = 0x20, KeyPageUp, KeyPageDown, KeyEnd, KeyHome, KeyArrowLeft,
        KeyArrowUp, KeyArrowRight, KeyArrowDown, KeySelect, KeyPrint, KeyExecute,
        KeyPrintScreen, KeyInsert, KeyDelete, KeyHelp,

        Key0, Key1, Key2, Key3, Key4, Key5, Key6, Key7, Key8, Key9,
        KeyA = 0x41, KeyB, KeyC, KeyD, KeyE, KeyF, KeyG, KeyH, KeyI, KeyJ, KeyK,
        KeyL, KeyM, KeyN, KeyO, KeyP, KeyQ, KeyR, KeyS, KeyT, KeyU, KeyV, KeyW,
        KeyX, KeyY, KeyZ, KeyLWin, KeyRWin, KeyApps,

        KeyNum0 = 0x60, KeyNum1, KeyNum2, KeyNum3, KeyNum4, KeyNum5, KeyNum6,
        KeyNum7, KeyNum8, KeyNum9, KeyNumMultiply, KeyNumAdd, KeyNumSeparator,
        KeyNumSubtract, KeyNumDecimal, KeyNumDivide,
        KeyF1, KeyF2, KeyF3, KeyF4, KeyF5, KeyF6, KeyF7, KeyF8, KeyF9, KeyF10,
        KeyF11, KeyF12, KeyF13, KeyF14, KeyF15, KeyF16, KeyF17, KeyF18, KeyF19,
        KeyF20, KeyF21, KeyF22, KeyF23, KeyF24,

        KeyNumLock = 0x90, KeyScroll,

        KeyLShift = 0xA0, KeyRShift, KeyLControl, KeyRControl, KeyLAlt, KeyRAlt,

        KeyBrowserBack, KeyBrowserForward, KeyBrowserRefresh, KeyBrowserStop,
        KeyBrowserSearch, KeyBrowserFavorite, KeyBrowserHome, KeyVolumeMute,
        KeyVolumeDown, KeyVolumeUp, KeyMediaNextTrack, KeyMediaPrevTrack,
        KeyMediaStop, KeyMediaPlayPause, KeyLaunchMail, KeyLaunchMediaSelect,
        KeyLaunchApp1, KeyLaunchApp2,

        KeyOEM_1 = 0xBA, KeyOEM_Plus, KeyOEM_Comma, KeyOEM_Minus, KeyOEM_Period,
        KeyOEM_2, KeyOEM_3, KeyOEM_4 = 0xDB, KeyOEM_5, KeyOEM_6, KeyOEM_7, KeyOEM_8,
        KeyOEM_102 = 0xE2,

        KeyProcessKey = 0xE5,
        KeyAttn = 0xF6, KeyCrSel, KeyExSel, KeyEraseEOF, KeyPlay, KeyZoom,

        KeyPA1 = 0xFD, KeyOEM_Clear
    };

    enum KeyMod
    {
        ModAlt = 0x1,
        ModControl = 0x2,
        ModShift = 0x4,
        ModWin = 0x8,
        ModNoRepeat = 0x4000
    };

    template<typename ... Mods>
    concept AllKeyMods = (std::is_same<Mods, KeyMod>::value && ...);

    enum class FontAlign
    {
        Leading = 0,
        Trailing = 1,
        Center = 2,
        Justified = 3
    };

    enum class FontWeight
    {
        Thin = 100, ExtraLight = 200, UltraLight = 200,
        Light = 300, SemiLight = 350, Normal = 400,
        Regular = 400, Medium = 500, DemiBold = 600,
        SemiBold = 600, Bold = 700, ExtraBold = 800,
        UltraBold = 800, Black = 900, Heavy = 900,
        ExtraBlack = 950, UltraBlack = 950
    };

    enum class FontStyle
    {
        Normal, Oblique, Italic
    };

    enum class FontStretch
    {
        Undefined, UltraCondensed, ExtraCondensed, Condensed,
        SemiCondensed, Normal, Medium = 5, SemiExpanded, Expanded,
        ExtraExpanded, UltraExpanded
    };

    // ----------------------------------- //
    // ------------- Widgets ------------- //
    // ----------------------------------- //

    struct FontCache
    {
        ComPtr<IDWriteTextFormat> format;

        FontCache();
        ~FontCache();

        void operator=(nullptr_t);
    };

    struct TextCache
    {
        // size_t version = 0;
        // win::ComPtr<win::d2d1::SolidColorBrush> brush;
        ComPtr<IDWriteTextLayout> layout;

        TextCache();
        ~TextCache();

        void operator=(nullptr_t);
    };

    // ------------------------- //

    struct IconFuture
    {
        ComPtr<ID2D1Bitmap> icon;
        std::mutex mutex;
        bool cancelled = false;
        uint8_t age = 0;

        ~IconFuture();
    };

    struct IconCache
    {
        std::shared_ptr<IconFuture> iconFuture;

        void operator=(nullptr_t);

        IconCache();
        ~IconCache();
    };
}