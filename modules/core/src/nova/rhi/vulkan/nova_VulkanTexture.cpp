#include "nova_VulkanRHI.hpp"

namespace nova
{
    Sampler Sampler::Create(HContext context, Filter filter, AddressMode addressMode, BorderColor color, f32 anisotropy)
    {
        auto impl = new Impl;
        impl->context = context;

        vkh::Check(vkCreateSampler(context->device, Temp(VkSamplerCreateInfo {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter = GetVulkanFilter(filter),
            .minFilter = GetVulkanFilter(filter),
            // TODO: Separate option
            .mipmapMode = filter == Filter::Linear
                ? VK_SAMPLER_MIPMAP_MODE_LINEAR
                : VK_SAMPLER_MIPMAP_MODE_NEAREST,
            .addressModeU = GetVulkanAddressMode(addressMode),
            .addressModeV = GetVulkanAddressMode(addressMode),
            .addressModeW = GetVulkanAddressMode(addressMode),
            .anisotropyEnable = anisotropy > 0.f,
            .maxAnisotropy = anisotropy,
            .maxLod = VK_LOD_CLAMP_NONE,
            .borderColor = GetVulkanBorderColor(color),
        }), context->pAlloc, &impl->sampler));

        return { impl };
    }

    void Sampler::Destroy()
    {
        if (!impl) {
            return;
        }

        vkDestroySampler(impl->context->device, impl->sampler, impl->context->pAlloc);

        delete impl;
        impl = nullptr;
    }

// -----------------------------------------------------------------------------

    Texture Texture::Create(HContext context, Vec3U size, TextureUsage usage, Format format, TextureFlags flags)
    {
        auto impl = new Impl;
        impl->context = context;
        impl->format = format;
        impl->usage = usage;
        bool makeView = (GetVulkanImageUsage(usage) & ~(VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)) != 0;

        impl->mips = flags >= TextureFlags::Mips
            ? 1 + u32(std::log2(f32(std::max(size.x, size.y))))
            : 1;

        VkImageType imageType;
        VkImageViewType viewType;
        impl->layers = 1;

        if (flags >= TextureFlags::Array) {
            if (size.z > 0) {
                impl->layers = size.z;
                size.z = 1;
                imageType = VK_IMAGE_TYPE_2D;
                viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;

            } else if (size.y > 0) {
                impl->layers = size.y;
                size.y = 1;
                imageType = VK_IMAGE_TYPE_1D;
                viewType = VK_IMAGE_VIEW_TYPE_1D_ARRAY;
            } else {
                NOVA_THROW("Image array must have at least 1 dimension and 1 layer");
            }
        } else {
            if (size.z > 0) {
                imageType = VK_IMAGE_TYPE_3D;
                viewType = VK_IMAGE_VIEW_TYPE_3D;

            } else if (size.y > 0) {
                imageType = VK_IMAGE_TYPE_2D;
                viewType = VK_IMAGE_VIEW_TYPE_2D;

            } else if (size.z > 0) {
                imageType = VK_IMAGE_TYPE_1D;
                viewType = VK_IMAGE_VIEW_TYPE_1D;
            } else {
                NOVA_THROW("Image must have at least one non-zero dimension");
            }
        }

        impl->extent = glm::max(size, Vec3U(1));

        // ---- Create image -----

        vkh::Check(vmaCreateImage(context->vma,
            Temp(VkImageCreateInfo {
                .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                .imageType = VK_IMAGE_TYPE_2D,
                .format = GetVulkanFormat(impl->format),
                .extent = { impl->extent.x, impl->extent.y, impl->extent.z },
                .mipLevels = impl->mips,
                .arrayLayers = impl->layers,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .tiling = VK_IMAGE_TILING_OPTIMAL,
                .usage = GetVulkanImageUsage(impl->usage)
                    | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT
                    | VK_IMAGE_USAGE_HOST_TRANSFER_BIT_EXT,
                .sharingMode = VK_SHARING_MODE_CONCURRENT,
                .queueFamilyIndexCount = 3,
                .pQueueFamilyIndices = std::array {
                    0u, 1u, 2u, // TODO
                }.data(),
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            }),
            Temp(VmaAllocationCreateInfo {
                .usage = VMA_MEMORY_USAGE_AUTO,
                .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
                    | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
            }),
            &impl->image,
            &impl->allocation,
            nullptr));

        // ---- Pick aspects -----

        switch (GetVulkanFormat(impl->format)) {
        break;case VK_FORMAT_S8_UINT:
            impl->aspect = VK_IMAGE_ASPECT_STENCIL_BIT;

        break;case VK_FORMAT_D16_UNORM:
              case VK_FORMAT_X8_D24_UNORM_PACK32:
              case VK_FORMAT_D32_SFLOAT:
            impl->aspect = VK_IMAGE_ASPECT_DEPTH_BIT;

        break;case VK_FORMAT_D16_UNORM_S8_UINT:
              case VK_FORMAT_D24_UNORM_S8_UINT:
              case VK_FORMAT_D32_SFLOAT_S8_UINT:
            impl->aspect = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;

        break;default:
            impl->aspect = VK_IMAGE_ASPECT_COLOR_BIT;
        }

        // ---- Make view -----

        if (makeView) {
            vkh::Check(vkCreateImageView(context->device, Temp(VkImageViewCreateInfo {
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .image = impl->image,
                .viewType = viewType,
                .format = GetVulkanFormat(impl->format),
                .subresourceRange = { impl->aspect, 0, impl->mips, 0, impl->layers },
            }), context->pAlloc, &impl->view));
        }

        return { impl };
    }

    void Texture::Destroy()
    {
        if (!impl) {
            return;
        }

        if (impl->view) {
            vkDestroyImageView(impl->context->device, impl->view, impl->context->pAlloc);
        }

        if (impl->allocation) {
            vmaDestroyImage(impl->context->vma, impl->image, impl->allocation);
        }

        delete impl;
        impl = nullptr;
    }

    Vec3U Texture::GetExtent() const
    {
        return impl->extent;
    }

    Format Texture::GetFormat() const
    {
        return impl->format;
    }

    void Texture::Set(Vec3I offset, Vec3U extent, const void* data) const
    {
        vkTransitionImageLayoutEXT(impl->context->device, 1, Temp(VkHostImageLayoutTransitionInfoEXT {
            .sType = VK_STRUCTURE_TYPE_HOST_IMAGE_LAYOUT_TRANSITION_INFO_EXT,
            .image = impl->image,
            .oldLayout = impl->layout,
            .newLayout = VK_IMAGE_LAYOUT_GENERAL,
            .subresourceRange{ impl->aspect, 0, impl->mips, 0, impl->layers },
        }));

        vkCopyMemoryToImageEXT(impl->context->device, Temp(VkCopyMemoryToImageInfoEXT {
            .sType = VK_STRUCTURE_TYPE_COPY_MEMORY_TO_IMAGE_INFO_EXT,
            .dstImage = impl->image,
            .dstImageLayout = VK_IMAGE_LAYOUT_GENERAL,
            .regionCount = 1,
            .pRegions = Temp(VkMemoryToImageCopyEXT {
                .sType = VK_STRUCTURE_TYPE_MEMORY_TO_IMAGE_COPY_EXT,
                .pHostPointer = data,
                .imageSubresource{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
                .imageOffset{ offset.x, offset.y, offset.z },
                .imageExtent{ extent.x, extent.y, extent.z },
            })
        }));

        impl->layout = VK_IMAGE_LAYOUT_GENERAL;
        impl->stage = VK_PIPELINE_STAGE_2_HOST_BIT;
    }

    void Texture::Transition(TextureLayout layout) const
    {
        vkTransitionImageLayoutEXT(impl->context->device, 1, Temp(VkHostImageLayoutTransitionInfoEXT {
            .sType = VK_STRUCTURE_TYPE_HOST_IMAGE_LAYOUT_TRANSITION_INFO_EXT,
            .image = impl->image,
            .oldLayout = impl->layout,
            .newLayout = GetVulkanImageLayout(layout),
            .subresourceRange{ impl->aspect, 0, impl->mips, 0, impl->layers },
        }));

        impl->layout = GetVulkanImageLayout(layout);
        impl->stage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    }

// -----------------------------------------------------------------------------

    void CommandList::Transition(HTexture texture,
        VkImageLayout newLayout, VkPipelineStageFlags2 newStages) const
    {
        vkCmdPipelineBarrier2(impl->buffer, Temp(VkDependencyInfo {
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = Temp(VkImageMemoryBarrier2 {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,

                .srcStageMask = texture->stage,
                .srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
                .dstStageMask = newStages,
                .dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,

                .oldLayout = texture->layout,
                .newLayout = newLayout,
                .image = texture->image,
                .subresourceRange{ texture->aspect, 0, texture->mips, 0, texture->layers },
            })
        }));

        texture->layout = newLayout;
        texture->stage = newStages;
    }

    void CommandList::Transition(HTexture texture, TextureLayout layout, PipelineStage stage) const
    {
        auto queue = impl->pool->queue;
        auto vkLayout = GetVulkanImageLayout(layout);
        auto vkStage = GetVulkanPipelineStage(stage) & queue->stages;

        Transition(texture, vkLayout, vkStage);

        vkCmdPipelineBarrier2(impl->buffer, Temp(VkDependencyInfo {
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = Temp(VkImageMemoryBarrier2 {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,

                .srcStageMask = texture->stage,
                .srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
                .dstStageMask = vkStage,
                .dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,

                .oldLayout = texture->layout,
                .newLayout = vkLayout,
                .image = texture->image,
                .subresourceRange{ texture->aspect, 0, texture->mips, 0, texture->layers },
            })
        }));

        texture->layout = vkLayout;
        texture->stage = vkStage;
    }

    void CommandList::ClearColor(HTexture texture, std::variant<Vec4, Vec4U, Vec4I> value) const
    {
        Transition(texture, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_2_CLEAR_BIT);

        vkCmdClearColorImage(impl->buffer,
            texture->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            nova::Temp(std::visit(Overloads {
                [&](const Vec4&  v) { return VkClearColorValue{ .float32{ v.r, v.g, v.b, v.a }}; },
                [&](const Vec4U& v) { return VkClearColorValue{  .uint32{ v.r, v.g, v.b, v.a }}; },
                [&](const Vec4I& v) { return VkClearColorValue{   .int32{ v.r, v.g, v.b, v.a }}; },
            }, value)),
            1, nova::Temp(VkImageSubresourceRange {
                VK_IMAGE_ASPECT_COLOR_BIT, 0, texture->mips, 0, texture->layers }));
    }

    void CommandList::CopyToTexture(HTexture dst, HBuffer src, u64 srcOffset) const
    {
        Transition(dst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_2_COPY_BIT);

        vkCmdCopyBufferToImage2(impl->buffer, Temp(VkCopyBufferToImageInfo2 {
            .sType = VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2,
            .srcBuffer = src->buffer,
            .dstImage = dst->image,
            .dstImageLayout = dst->layout,
            .regionCount = 1,
            .pRegions = Temp(VkBufferImageCopy2 {
                .sType = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2,
                .bufferOffset = srcOffset,
                .imageSubresource{ dst->aspect, 0, 0, dst->layers },
                .imageExtent{ dst->extent.x, dst->extent.y,  1 },
            }),
        }));
    }

    void CommandList::CopyFromTexture(HBuffer dst, HTexture src, Rect2D region) const
    {
        Transition(src,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT);
        vkCmdCopyImageToBuffer2(impl->buffer, nova::Temp(VkCopyImageToBufferInfo2 {
            .sType = VK_STRUCTURE_TYPE_COPY_IMAGE_TO_BUFFER_INFO_2,
            .srcImage = src->image,
            .srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            .dstBuffer = dst->buffer,
            .regionCount = 1,
            .pRegions = nova::Temp(VkBufferImageCopy2 {
                .sType = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2,
                .imageSubresource{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
                .imageOffset{ region.offset.x, region.offset.y, 0 },
                .imageExtent{ region.extent.x, region.extent.y, 1 },
            }),
        }));
    }

    void CommandList::GenerateMips(HTexture texture) const
    {
        if (texture->mips == 1) {
            return;
        }

        Transition(texture, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT);

        int32_t mipWidth = texture->extent.x;
        int32_t mipHeight = texture->extent.y;

        for (uint32_t mip = 1; mip < texture->mips; ++mip) {
            vkCmdPipelineBarrier2(impl->buffer, Temp(VkDependencyInfo {
                .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .imageMemoryBarrierCount = 1,
                .pImageMemoryBarriers = Temp(VkImageMemoryBarrier2 {
                    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,

                    .srcStageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
                    .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    .dstStageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
                    .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT | VK_ACCESS_2_TRANSFER_READ_BIT,

                    .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    .image = texture->image,
                    .subresourceRange = { texture->aspect, mip - 1, 1, 0, 1 },
                }),
            }));

            vkCmdBlitImage(impl->buffer,
                texture->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                texture->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1, Temp(VkImageBlit {
                    .srcSubresource{ VK_IMAGE_ASPECT_COLOR_BIT, mip - 1, 0, 1 },
                    .srcOffsets{ VkOffset3D{}, VkOffset3D{(int32_t)mipWidth, (int32_t)mipHeight, 1} },
                    .dstSubresource{ VK_IMAGE_ASPECT_COLOR_BIT, mip, 0, 1 },
                    .dstOffsets{ VkOffset3D{}, VkOffset3D{(int32_t)std::max(mipWidth / 2, 1), (int32_t)std::max(mipHeight / 2, 1), 1} },
                }),
                VK_FILTER_LINEAR);

            mipWidth = std::max(mipWidth / 2, 1);
            mipHeight = std::max(mipHeight / 2, 1);
        }

        vkCmdPipelineBarrier2(impl->buffer, Temp(VkDependencyInfo {
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = Temp(VkImageMemoryBarrier2 {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,

                .srcStageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
                .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
                .dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT,

                .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                .image = texture->image,
                .subresourceRange{ texture->aspect, texture->mips - 1, 1, 0, 1 },
            }),
        }));

        texture->layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    }

    void CommandList::BlitImage(HTexture dst, HTexture src, Filter filter) const
    {
        Transition(src, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_2_BLIT_BIT);
        Transition(dst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_2_BLIT_BIT);

        vkCmdBlitImage(impl->buffer,
            src->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            dst->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, nova::Temp(VkImageBlit {
                .srcSubresource{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
                .srcOffsets{ VkOffset3D{}, VkOffset3D{i32(src->extent.x), i32(src->extent.y), 1} },
                .dstSubresource{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
                .dstOffsets{ VkOffset3D{}, VkOffset3D{i32(dst->extent.x), i32(dst->extent.y), 1} },
            }),
            GetVulkanFilter(filter));
    }
}