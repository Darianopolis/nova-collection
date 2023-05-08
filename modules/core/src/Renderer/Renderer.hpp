#pragma once

#include <VulkanBackend/VulkanBackend.hpp>

namespace pyr
{
    enum class MeshID         : u32 {};
    enum class MaterialTypeID : u32 {};
    enum class MaterialID     : u32 {};
    enum class ObjectID       : u32 {};
    enum class TextureID      : u32 {};

    struct RasterPushConstants
    {
        mat4 mvp;
        u64 vertices;
        u64 material;
        u64 vertexOffset;
        u32 vertexStride;
    };

    struct Mesh
    {
        Buffer vertices;
        Buffer indices;
        u32 indexCount;
        u64 vertexOffset;
        u32 vertexStride;
    };

    struct MaterialType
    {
        Shader shader;
        b8 inlineData;
    };

    struct Material
    {
        MaterialTypeID materialTypeID;
        u64 data;
    };

    struct Texture
    {
        u32 index;
    };

    struct Object
    {
        vec3 position;
        quat rotation;
        vec3 scale;
        MeshID meshID;
        MaterialID materialID;
    };

    template<class Element, class Key>
    struct Registry
    {
        enum class ElementFlag : u8
        {
            Empty = 0,
            Exists = 1,
        };
        std::vector<ElementFlag> flags;
        std::vector<Element> elements;
        std::vector<Key> freelist;

        std::pair<Key, Element&> Acquire()
        {
            if (freelist.empty())
            {
                auto& element = elements.emplace_back();
                flags.push_back(ElementFlag::Exists);
                return { Key(elements.size() - 1), element };
            }

            Key key = freelist.back();
            freelist.pop_back();
            flags[static_cast<std::underlying_type_t<Key>>(key)] = ElementFlag::Exists;
            return { key, Get(key) };
        }

        void Return(Key key)
        {
            flags[static_cast<std::underlying_type_t<Key>>(key)] = ElementFlag::Empty;
            freelist.push_back(key);
        }

        Element& Get(Key key)
        {
            return elements[static_cast<std::underlying_type_t<Key>>(key)];
        }

        template<class Fn>
        void ForEach(Fn&& visit)
        {
            for (u32 i = 0; i < elements.size(); ++i)
            {
                if (flags[i] == ElementFlag::Exists)
                    visit(Key(i), elements[i]);
            }
        }
    };

    struct Renderer
    {
        static constexpr u32 MaxMaterials = 65'536;
        static constexpr u32 MaterialSize = 64;
    public:
        Context* ctx = {};

        Image depthBuffer;
        uvec3 lastExtent;

        VkPipelineLayout layout;
        Shader vertexShader;

        Buffer materialBuffer;

        VkDescriptorSetLayout textureDescriptorSetLayout;
        VkDescriptorSet textureDescriptorSet;
        Buffer textureDescriptorBuffer;

        Registry<Mesh, MeshID> meshes;
        Registry<Object, ObjectID> objects;
        Registry<MaterialType, MaterialTypeID> materialTypes;
        Registry<Material, MaterialID> materials;
        Registry<Texture, TextureID> textures;

        vec3 viewPosition = vec3(0.f, 0.f, 0.f);
        quat viewRotation = vec3(0.f);
        f32 viewFov = glm::radians(90.f);
    public:
        void Init(Context& ctx);

        MeshID CreateMesh(
            usz dataSize, const void* pData,
            u32 vertexStride, usz vertexOffset,
            u32 indexCount, const u32* pIndices);
        void DeleteMesh(MeshID);

        MaterialTypeID CreateMaterialType(const char* pShader, b8 inlineData);
        void DeleteMaterialType(MaterialTypeID);

        MaterialID CreateMaterial(MaterialTypeID matTypeID, const void* pData, usz size);
        void DeleteMaterial(MaterialID);

        ObjectID CreateObject(MeshID meshID, MaterialID matID, vec3 position, quat rotation, vec3 scale);
        void DeleteObject(ObjectID);

        TextureID RegisterTexture(VkImageView view, VkSampler sampler);
        void UnregisterTexture(TextureID);

        void SetCamera(vec3 position, quat rotation, f32 fov);
        void Draw(Image& target);
    };
}
