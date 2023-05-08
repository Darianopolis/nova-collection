#include "Renderer.hpp"

namespace pyr
{
    static mat4 ProjInfReversedZRH(f32 fovY, f32 aspectWbyH, f32 zNear)
    {
        // https://nlguillemot.wordpress.com/2016/12/07/reversed-z-in-opengl/

        f32 f = 1.f / glm::tan(fovY / 2.f);
        mat4 proj{};
        proj[0][0] = f / aspectWbyH;
        proj[1][1] = f;
        proj[3][2] = zNear; // Right, middle-bottom
        proj[2][3] = -1.f;  // Bottom, middle-right
        return proj;
    }

    void Renderer::Init(Context& _ctx)
    {
        ctx = &_ctx;

// -----------------------------------------------------------------------------

        VkCall(vkCreateDescriptorSetLayout(ctx->device, Temp(VkDescriptorSetLayoutCreateInfo {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext = Temp(VkDescriptorSetLayoutBindingFlagsCreateInfo {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
                .bindingCount = 1,
                .pBindingFlags = Temp<VkDescriptorBindingFlags>(VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT),
            }),
            .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT,
            .bindingCount = 1,
            .pBindings = Temp(VkDescriptorSetLayoutBinding {
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 65'536,
                .stageFlags = VK_SHADER_STAGE_ALL,
            }),
        }), nullptr, &textureDescriptorSetLayout));

        VkDeviceSize descriptorSize;
        vkGetDescriptorSetLayoutSizeEXT(ctx->device, textureDescriptorSetLayout, &descriptorSize);
        textureDescriptorBuffer = ctx->CreateBuffer(descriptorSize,
            VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT, BufferFlags::DeviceLocal | BufferFlags::CreateMapped);

// -----------------------------------------------------------------------------

        vertexShader = ctx->CreateShader(
            VK_SHADER_STAGE_VERTEX_BIT, VK_SHADER_STAGE_FRAGMENT_BIT,
            "assets/shaders/vertex-generated",
            R"(
#version 460

#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64  : require
#extension GL_EXT_buffer_reference2 : require

layout(buffer_reference, scalar) buffer PositionBR { vec3 value[]; };

layout(push_constant) uniform PushConstants
{
    mat4 mvp;
    uint64_t vertices;
    uint64_t material;
    uint64_t vertexOffset;
    uint vertexStride;
} pc;

layout(location = 0) out uint outVertexIndex;

void main()
{
    vec3 pos = PositionBR(pc.vertices + pc.vertexOffset + (gl_VertexIndex * pc.vertexStride)).value[0];
    outVertexIndex = gl_VertexIndex;
    gl_Position = pc.mvp * vec4(pos, 1);
}
            )",
            {{
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                .size = sizeof(RasterPushConstants),
            }},
            {
                textureDescriptorSetLayout
            });

// -----------------------------------------------------------------------------

        materialBuffer = ctx->CreateBuffer(MaxMaterials * MaterialSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, BufferFlags::DeviceLocal | BufferFlags::CreateMapped);

// -----------------------------------------------------------------------------

        VkCall(vkCreatePipelineLayout(ctx->device, Temp(VkPipelineLayoutCreateInfo {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 1,
            .pSetLayouts = &textureDescriptorSetLayout,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = Temp(VkPushConstantRange {
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                .size = sizeof(RasterPushConstants),
            }),
        }), nullptr, &layout));
    }

    void Renderer::SetCamera(vec3 position, quat rotation, f32 fov)
    {
        viewPosition = position;
        viewRotation = rotation;
        viewFov = fov;
    }

    void Renderer::Draw(Image& target)
    {
        auto cmd = ctx->cmd;

        // Resize depth buffer

        if (target.extent != lastExtent)
        {
            lastExtent = target.extent;

            ctx->DestroyImage(depthBuffer);
            depthBuffer = ctx->CreateImage(target.extent, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_FORMAT_X8_D24_UNORM_PACK32);
            ctx->Transition(cmd, depthBuffer, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
        }

        // Begin rendering

        ctx->Transition(cmd, target, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        vkCmdBeginRendering(cmd, Temp(VkRenderingInfo {
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .renderArea = { {}, { target.extent.x, target.extent.y } },
            .layerCount = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments = Temp(VkRenderingAttachmentInfo {
                .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .imageView = target.view,
                .imageLayout = target.layout,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .clearValue = {{{ 0.1f, 0.1f, 0.1f, 1.f }}},
            }),
            .pDepthAttachment = Temp(VkRenderingAttachmentInfo {
                .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .imageView = depthBuffer.view,
                .imageLayout = depthBuffer.layout,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .clearValue = { .depthStencil = {{ 0.f }} },
            })
        }));

        vkCmdSetScissorWithCount(cmd, 1, Temp(VkRect2D { {}, { target.extent.x, target.extent.y } }));
        vkCmdSetViewportWithCount(cmd, 1, Temp(VkViewport {
            0.f, f32(target.extent.y),
            f32(target.extent.x), -f32(target.extent.y),
            0.f, 1.f
        }));

        // Set blend, depth, cull dynamic state

        b8 blend = true;
        b8 depth = true;
        b8 cull = false;

        if (depth)
        {
            vkCmdSetDepthTestEnable(cmd, true);
            vkCmdSetDepthWriteEnable(cmd, true);
            vkCmdSetDepthCompareOp(cmd, VK_COMPARE_OP_GREATER);
        }

        if (cull)
        {
            vkCmdSetCullMode(cmd, VK_CULL_MODE_BACK_BIT);
            vkCmdSetFrontFace(cmd, VK_FRONT_FACE_COUNTER_CLOCKWISE);
        }

        vkCmdSetColorWriteMaskEXT(cmd, 0, 1, Temp<VkColorComponentFlags>(
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
        ));
        if (blend)
        {
            vkCmdSetColorBlendEnableEXT(cmd, 0, 1, Temp<VkBool32>(true));
            vkCmdSetColorBlendEquationEXT(cmd, 0, 1, Temp(VkColorBlendEquationEXT {
                .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
                .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                .colorBlendOp = VK_BLEND_OP_ADD,
                .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                .alphaBlendOp = VK_BLEND_OP_ADD,
            }));
        }
        else
        {
            vkCmdSetColorBlendEnableEXT(cmd, 0, 1, Temp<VkBool32>(false));
        }

        // Bind shaders and draw

        auto proj = ProjInfReversedZRH(viewFov, f32(target.extent.x) / f32(target.extent.y), 0.01f);
        auto translate = glm::translate(mat4(1.f), viewPosition);
        auto rotate = glm::mat4_cast(viewRotation);
        auto viewProj = proj * glm::affineInverse(translate * rotate);

        vkCmdSetPrimitiveTopology(cmd, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

        vkCmdBindDescriptorBuffersEXT(cmd, 1, Temp(VkDescriptorBufferBindingInfoEXT {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT,
            .address = textureDescriptorBuffer.address,
            .usage = VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT,
        }));
        vkCmdSetDescriptorBufferOffsetsEXT(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, Temp(0u), Temp(0ull));

        objects.ForEach([&](auto, auto& object) {
            auto& mesh = meshes.Get(object.meshID);

            auto transform = glm::translate(glm::mat4(1.f), object.position);
            transform *= glm::mat4_cast(object.rotation);
            transform *= glm::scale(glm::mat4(1.f), object.scale);

            auto& material = materials.Get(object.materialID);
            auto& materialType = materialTypes.Get(material.materialTypeID);

            vkCmdBindShadersEXT(cmd, 2,
                std::array { vertexShader.stage, materialType.shader.stage }.data(),
                std::array { vertexShader.shader, materialType.shader.shader }.data());
            vkCmdBindIndexBuffer(cmd, mesh.indices.buffer, 0, VK_INDEX_TYPE_UINT32);
            vkCmdPushConstants(cmd, layout,
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                0, sizeof(RasterPushConstants), Temp(RasterPushConstants {
                    .mvp = viewProj * transform,
                    .vertices = mesh.vertices.address,
                    .material = material.data,
                    .vertexOffset = mesh.vertexOffset,
                    .vertexStride = mesh.vertexStride,
                }));

            vkCmdDrawIndexed(cmd, mesh.indexCount, 1, 0, 0, 0);
        });

        // End rendering

        vkCmdEndRendering(cmd);
    }

    MeshID Renderer::CreateMesh(
        usz dataSize, const void* pData,
        u32 vertexStride, usz vertexOffset,
        u32 indexCount, const u32* pIndices)
    {
        auto[id, mesh] = meshes.Acquire();

        mesh.vertices = ctx->CreateBuffer(dataSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, BufferFlags::DeviceLocal);
        ctx->CopyToBuffer(mesh.vertices, pData, dataSize);

        mesh.vertexOffset = vertexOffset;
        mesh.vertexStride = vertexStride;

        auto indexSize = sizeof(u32) * indexCount;
        mesh.indices = ctx->CreateBuffer(indexSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, BufferFlags::DeviceLocal);
        ctx->CopyToBuffer(mesh.indices, pIndices, indexSize);

        mesh.indexCount = indexCount;

        return id;
    }

    void Renderer::DeleteMesh(MeshID id)
    {
        auto& mesh = meshes.Get(id);
        ctx->DestroyBuffer(mesh.vertices);
        ctx->DestroyBuffer(mesh.indices);
        meshes.Return(id);
    }

    MaterialTypeID Renderer::CreateMaterialType(const char* pShader, b8 inlineData)
    {
        auto[id, materialType] = materialTypes.Acquire();
        materialType.shader = ctx->CreateShader(
            VK_SHADER_STAGE_FRAGMENT_BIT, 0,
            "assets/shaders/fragment-generated",
            std::format("{}{}{}",
            R"(
#version 460

#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64  : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_fragment_shader_barycentric : require
#extension GL_EXT_nonuniform_qualifier : require

layout(set = 0, binding = 0) uniform sampler2D textures[];

layout(location = 0) in pervertexEXT uint vertexIndex[3];

layout(location = 0) out vec4 fragColor;

layout(push_constant) uniform PushConstants
{
    mat4 mvp;
    uint64_t vertices;
    uint64_t material;
    uint64_t vertexOffset;
    uint vertexStride;
} pc;
            )",
            pShader,
            R"(
void main()
{
    fragColor = shade(
        pc.vertices, pc.material,
        uvec3(vertexIndex[0], vertexIndex[1], vertexIndex[2]),
        gl_BaryCoordEXT);
}
            )"),
            {{
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                .size = sizeof(RasterPushConstants),
            }},
            {
                textureDescriptorSetLayout
            });
        materialType.inlineData = inlineData;

        return id;
    }

    void Renderer::DeleteMaterialType(MaterialTypeID id)
    {
        auto& material = materialTypes.Get(id);
        ctx->DestroyShader(material.shader);
        materialTypes.Return(id);
    }

    MaterialID Renderer::CreateMaterial(MaterialTypeID matTypeID, const void* pData, usz size)
    {
        auto[id, material] = materials.Acquire();
        material.materialTypeID = matTypeID;

        if (pData && size > 0)
        {
            if (materialTypes.Get(matTypeID).inlineData)
            {
                if (size > 8)
                    PYR_THROW("Inline material data must not exceed 8 bytes");

                std::memcpy(&material.data, pData, size);
            }
            else
            {
                if (size > 64)
                    PYR_THROW("Material size [{}] exceeds max (64)", size);

                // TODO: Better material allocation strategy
                //  - don't consume buffer space for inline materials
                //  - allow packaged varaible size materials (16, 32, 64)

                u64 offset = MaterialSize * u32(id);
                std::memcpy(materialBuffer.mapped + offset, pData, size);
                material.data = materialBuffer.address + offset;
            }
        }

        return id;
    }

    void Renderer::DeleteMaterial(MaterialID id)
    {
        materials.Return(id);
    }

    ObjectID Renderer::CreateObject(MeshID meshID, MaterialID materialID, vec3 position, quat rotation, vec3 scale)
    {
        auto[id, object] = objects.Acquire();
        object.meshID = meshID;
        object.materialID = materialID;
        object.position = position;
        object.rotation = rotation;
        object.scale = scale;

        return id;
    }

    void Renderer::DeleteObject(ObjectID id)
    {
        objects.Return(id);
    }

    TextureID Renderer::RegisterTexture(VkImageView view, VkSampler sampler)
    {
        auto[id, texture] = textures.Acquire();
        texture.index = u32(id);

        vkGetDescriptorEXT(ctx->device,
            Temp(VkDescriptorGetInfoEXT {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT,
                .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .data = {
                    .pCombinedImageSampler = Temp(VkDescriptorImageInfo {
                        .sampler = sampler,
                        .imageView = view,
                        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, // TODO: configurable?
                    }),
                },
            }),
            ctx->descriptorSizes.combinedImageSamplerDescriptorSize,
            textureDescriptorBuffer.mapped + (u32(id) * ctx->descriptorSizes.combinedImageSamplerDescriptorSize));

        return id;
    }

    void Renderer::UnregisterTexture(TextureID id)
    {
        textures.Return(id);
    }
}