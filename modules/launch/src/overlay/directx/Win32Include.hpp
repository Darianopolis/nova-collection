#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <windowsx.h>
#include <d2d1.h>
#include <dwrite.h>
#include <shellapi.h>
#include <wincodec.h>
#include <CommCtrl.h>

#pragma warning(push)
#pragma warning(disable: 4265)
#include <wrl.h>
using namespace Microsoft::WRL;
#pragma warning(pop)
