#pragma once

#include <nova/rhi/nova_RHI.hpp>
#include <nova/core/nova_Timer.hpp>

#include <nova/imdraw/nova_ImDraw2D.hpp>

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <windowsx.h>
#include <shellapi.h>
#include <wincodec.h>
#include <CommCtrl.h>
#include <shellapi.h>
#include <combaseapi.h>

using namespace nova::types;

namespace nms
{
    void ConvertToWString(std::string_view input, std::wstring& output);
    inline std::wstring ConvertToWString(std::string_view input)
    {
        std::wstring str;
        ConvertToWString(input, str);
        return str;
    }

    void ConvertToString(std::wstring_view input, std::string& output);
    inline std::string ConvertToString(std::wstring_view input)
    {
        NOVA_LOG("Converting to std::string, input len = {}", input.size());
        std::wcout << std::format(L"  Input = {}", input);
        std::string str;
        ConvertToString(input, str);
        NOVA_LOG("  Output = {}, len = {}", str, str.size());
        return str;
    }

    nova::Texture* LoadIconFromPath(
        nova::Context* context,
        nova::CommandPool* cmdPool,
        nova::ResourceTracker* tracker,
        nova::Queue* queue,
        nova::Fence* fence,
        std::string_view path);
}