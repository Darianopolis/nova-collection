#pragma once

#include <VulkanBackend/VulkanBackend.hpp>

namespace pyr
{
    struct GltfVertex
    {
        vec3 position = vec3(0.f);
        vec3 normal = vec3(0.f);
        vec2 texCoord = vec2(0.f);
        u32 texIndex = 0;
    };

    struct GltfMesh
    {
        std::vector<GltfVertex> vertices;
        std::vector<u32> indices;
        std::vector<Ref<Image>> images;
    };

    GltfMesh LoadMesh(Context& ctx, const char* file, const char* baseDir);
}