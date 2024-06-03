#pragma once

#include <nova/rhi/nova_RHI.hpp>

using namespace nova::types;

namespace nms
{
    nova::Image LoadIconFromPath(
        nova::Context context,
        std::string_view path);

    void ClearIconCache();
}