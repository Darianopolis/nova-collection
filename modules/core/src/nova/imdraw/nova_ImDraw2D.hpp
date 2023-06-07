#pragma once

#include <nova/rhi/nova_RHI.hpp>

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
        Texture texture;
        ImTextureID index;
        f32 width;
        f32 height;
        f32 advance;
        Vec2 offset;
    };

// -----------------------------------------------------------------------------

    struct ImFont : ImplHandle<struct ImFontImpl> {};

    struct ImDraw2D : ImplHandle<struct ImDraw2DImpl>
    {
        ImDraw2D() = default;
        ImDraw2D(Context context);

        Sampler GetDefaultSampler() const noexcept;
        const ImBounds2D& GetBounds() const noexcept;

        ImTextureID RegisterTexture(Texture texture, Sampler sampler) const;
        void UnregisterTexture(ImTextureID textureSlot) const;

        ImFont LoadFont(const char* file, f32 size, CommandPool cmdPool, CommandState state, Fence fence, Queue queue) const;

        void Reset() const;
        void DrawRect(const ImRoundRect& rect) const;
        void DrawString(std::string_view str, Vec2 pos, ImFont font) const;

        ImBounds2D MeasureString(std::string_view str, ImFont font) const;

        void Record(CommandList commandList) const;
    };

// -----------------------------------------------------------------------------

    struct ImFontImpl : ImplBase
    {
        ImDraw2D imDraw;
        std::vector<ImGlyph> glyphs;

    public:
        ~ImFontImpl();
    };

    struct ImDraw2DImpl : ImplBase
    {
        struct PushConstants
        {
            alignas(8) Vec2 invHalfExtent;
            alignas(8) Vec2 centerPos;
            u64 rectInstancesVA;
        };

        static constexpr VkPushConstantRange PushConstantsRange {
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            .size = sizeof(PushConstants),
        };

        static constexpr u32 MaxPrimitives = 65'536;

    public:
        Context context = {};

        Sampler defaultSampler = {};

        PipelineLayout pipelineLayout = {};

        DescriptorSetLayout  descriptorSetLayout = {};
        // Buffer               descriptorBuffer = {};
        DescriptorSet              descriptorSet = {};
        u32                      nextTextureSlot = 0;
        std::vector<u32>     textureSlotFreelist = {};

        Shader rectVertShader = {};
        Shader rectFragShader = {};
        Buffer     rectBuffer = {};
        u32         rectIndex = 0;

        ImBounds2D bounds;

        std::vector<ImDrawCommand> drawCommands;
    };
}