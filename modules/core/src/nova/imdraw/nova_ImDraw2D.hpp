#pragma once

#include <nova/rhi/vulkan/nova_VulkanRHI.hpp>

namespace nova
{
    enum class ImTextureID : u32 {};

    struct ImBounds2D
    {
        Vec2 min {  INFINITY,  INFINITY };
        Vec2 max { -INFINITY, -INFINITY };

        void Expand(const ImBounds2D& other) noexcept
        {
            min.x = std::min(min.x, other.min.x);
            min.y = std::min(min.y, other.min.y);
            max.x = std::max(max.x, other.max.x);
            max.y = std::max(max.y, other.max.y);
        }

        Vec2 Size() const noexcept { return max - min; }
        Vec2 Center() const noexcept { return 0.5f * (max + min); }

        float Width()  const noexcept { return max.x - min.x; }
        float Height() const noexcept { return max.y - min.y; }

        bool Empty() const noexcept { return min.y == INFINITY; }
    };


    struct ImRoundRect
    {
        Vec4 centerColor;
        Vec4 borderColor;

        Vec2 centerPos;
        Vec2 halfExtent;

        f32 cornerRadius;
        f32 borderWidth;

        Vec4 texTint;
        ImTextureID texIndex;
        Vec2 texCenterPos;
        Vec2 texHalfExtent;

        static constexpr auto Layout = std::array {
            Member("centerColor", nova::ShaderVarType::Vec4),
            Member("borderColor", nova::ShaderVarType::Vec4),

            Member("centerPos",  nova::ShaderVarType::Vec2),
            Member("halfExtent", nova::ShaderVarType::Vec2),

            Member("cornerRadius", nova::ShaderVarType::F32),
            Member("borderWidth",  nova::ShaderVarType::F32),

            Member("texTint",       nova::ShaderVarType::Vec4),
            Member("texIndex",      nova::ShaderVarType::U32),
            Member("texCenterPos",  nova::ShaderVarType::Vec2),
            Member("texHalfExtent", nova::ShaderVarType::Vec2),
        };
    };

    enum class ImDrawType
    {
        RoundRect,
    };

    struct ImDrawCommand
    {
        ImDrawType type;
        u32 first;
        u32 count;
    };

// -----------------------------------------------------------------------------

    struct ImGlyph
    {
        std::unique_ptr<Texture> texture;
        ImTextureID index;
        f32 width;
        f32 height;
        f32 advance;
        Vec2 offset;
    };

// -----------------------------------------------------------------------------

    struct ImDraw2D;

    struct ImFont
    {
        ImDraw2D* imDraw;
        std::vector<ImGlyph> glyphs;

    public:
        ~ImFont();
    };

    struct ImDraw2D
    {
        struct PushConstants
        {
            Vec2 invHalfExtent;
            Vec2 centerPos;
            u64 rectInstancesVA;

            static constexpr auto Layout = std::array {
                Member("invHalfExtent",   nova::ShaderVarType::Vec2),
                Member("centerPos",       nova::ShaderVarType::Vec2),
                Member("rectInstancesVA", nova::ShaderVarType::U64),
            };
        };

        static constexpr u32 MaxPrimitives = 65'536;

    public:
        HContext context = {};

        Sampler defaultSampler;

        PipelineLayout pipelineLayout;

        DescriptorSetLayout descriptorSetLayout;
        DescriptorSet             descriptorSet;
        u32                      nextTextureSlot = 0;
        std::vector<u32>     textureSlotFreelist = {};

        Shader rectVertShader;
        Shader rectFragShader;
        Buffer     rectBuffer;
        u32         rectIndex = 0;

        ImBounds2D bounds;

        std::vector<ImDrawCommand> drawCommands;

    public:
        ImDraw2D(HContext context);
        ~ImDraw2D();

        HSampler GetDefaultSampler() noexcept;
        const ImBounds2D& GetBounds() const noexcept;

        ImTextureID RegisterTexture(HTexture texture, HSampler sampler);
        void UnregisterTexture(ImTextureID textureSlot);

        std::unique_ptr<ImFont> LoadFont(const char* file, f32 size, HCommandPool cmdPool, HCommandState state, HFence fence, HQueue queue);

        void Reset();
        void DrawRect(const ImRoundRect& rect);
        void DrawString(std::string_view str, Vec2 pos, ImFont& font);

        ImBounds2D MeasureString(std::string_view str, ImFont& font);

        void Record(HCommandList);
    };
}