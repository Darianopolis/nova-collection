#include "nova_VulkanRHI.hpp"

namespace nova
{
    static
    VkBool32 VKAPI_CALL DebugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT severity,
        VkDebugUtilsMessageTypeFlagsEXT type,
        const VkDebugUtilsMessengerCallbackDataEXT* data,
        [[maybe_unused]] void* userData)
    {
        if (severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
                || type != VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) {
            return VK_FALSE;
        }

        NOVA_LOG(R"(
--------------------------------------------------------------------------------)");
        std::cout << std::stacktrace::current();
        NOVA_LOG(R"(
--------------------------------------------------------------------------------
Validation: {} ({})
--------------------------------------------------------------------------------
{}
--------------------------------------------------------------------------------
)",
            data->pMessageIdName,
            data->messageIdNumber,
            data->pMessage);

        return VK_FALSE;
    }

    struct VulkanFeatureChain
    {
        ankerl::unordered_dense::map<VkStructureType, VkBaseInStructure*> deviceFeatures;
        ankerl::unordered_dense::set<std::string> extensions;
        VkBaseInStructure* pNext = nullptr;

        ~VulkanFeatureChain()
        {
            for (auto&[type, feature] : deviceFeatures) {
                mi_free(feature);
            }
        }

        void Extension(std::string name)
        {
            extensions.emplace(std::move(name));
        }

        template<typename T>
        T& Feature(VkStructureType type)
        {
            auto& f = deviceFeatures[type];
            if (!f) {
                f = static_cast<VkBaseInStructure*>(mi_malloc(sizeof(T)));
                new(f) T{};
                NOVA_LOG("Setting type: {} ({}) @ {} ({})", u32(type), i32(type), (void*)f, typeid(T).name());
                f->sType = type;
                f->pNext = pNext;
                pNext = f;
            }

            return *(T*)f;
        }

        const void* Build()
        {
            return pNext;
        }
    };

    Context Context::Create(const ContextConfig& config)
    {
        auto impl = new Impl;
        impl->config = config;

        std::vector<const char*> instanceLayers;
        if (config.debug) {
            instanceLayers.push_back("VK_LAYER_KHRONOS_validation");
        }

        std::vector<const char*> instanceExtensions = { VK_KHR_SURFACE_EXTENSION_NAME };
    #ifdef _WIN32
        instanceExtensions.push_back("VK_KHR_win32_surface");
    #endif
        if (config.debug) {
            instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            instanceExtensions.push_back(VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME);
        }

        volkInitialize();

        VkDebugUtilsMessengerCreateInfoEXT debugMessengerCreateInfo {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
                                | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
                                | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
            .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
                            | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                            | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
            .pfnUserCallback = DebugCallback,
            .pUserData = impl,
        };

        std::vector<VkValidationFeatureEnableEXT> validationFeaturesEnabled {
            // VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT,
            // VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT,
            // VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT,
        };

        VkValidationFeaturesEXT validationFeatures {
            .sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT,
            .pNext = &debugMessengerCreateInfo,
            .enabledValidationFeatureCount = u32(validationFeaturesEnabled.size()),
            .pEnabledValidationFeatures = validationFeaturesEnabled.data(),
        };

        VkCall(vkCreateInstance(Temp(VkInstanceCreateInfo {
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pNext = config.debug ? &validationFeatures : nullptr,
            .pApplicationInfo = Temp(VkApplicationInfo {
                .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                .apiVersion = VK_API_VERSION_1_3,
            }),
            .enabledLayerCount = u32(instanceLayers.size()),
            .ppEnabledLayerNames = instanceLayers.data(),
            .enabledExtensionCount = u32(instanceExtensions.size()),
            .ppEnabledExtensionNames = instanceExtensions.data(),
        }), impl->pAlloc, &impl->instance));

        volkLoadInstanceOnly(impl->instance);

        if (config.debug)
            VkCall(vkCreateDebugUtilsMessengerEXT(impl->instance, &debugMessengerCreateInfo, impl->pAlloc, &impl->debugMessenger));

        std::vector<VkPhysicalDevice> gpus;
        VkQuery(gpus, vkEnumeratePhysicalDevices, impl->instance);
        for (auto& gpu : gpus) {
            VkPhysicalDeviceProperties2 properties { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
            vkGetPhysicalDeviceProperties2(gpu, &properties);
            if (properties.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                impl->gpu = gpu;
                break;
            }
        }
        // u32 gpuCount = 1;
        // VkCall(vkEnumeratePhysicalDevices(impl->instance, &gpuCount, &impl->gpu));
        // if (gpuCount == 0)
        //     NOVA_THROW("No physical devices found!");

        // ---- Logical Device ----

        std::array<VkQueueFamilyProperties, 3> properties;
        vkGetPhysicalDeviceQueueFamilyProperties(impl->gpu, Temp(3u), properties.data());
        for (u32 i = 0; i < 16; ++i) {
            auto queue = new Queue::Impl;
            queue->context = { impl };
            queue->stages = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
            queue->family = 0;
            impl->graphicQueues.emplace_back(queue);
        }
        for (u32 i = 0; i < 2; ++i) {
            auto queue = new Queue::Impl;
            queue->context = { impl };
            queue->stages = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT;
            queue->family = 1;
            impl->transferQueues.emplace_back(queue);
        }
        for (u32 i = 0; i < 8; ++i) {
            auto queue = new Queue::Impl;
            queue->context = { impl };
            queue->stages = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT;
            queue->family = 2;
            impl->computeQueues.emplace_back(queue);
        }
        NOVA_LOGEXPR(impl->graphicQueues.size());
        NOVA_LOGEXPR(impl->graphicQueues.front().impl);

        VulkanFeatureChain chain;

        // TODO: Allow for optional features

        {
            chain.Extension(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
            chain.Extension(VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME);

            auto& f2 = chain.Feature<VkPhysicalDeviceFeatures2>(
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2);
            f2.features.wideLines = VK_TRUE;
            f2.features.shaderInt16 = VK_TRUE;
            f2.features.shaderInt64 = VK_TRUE;
            f2.features.fillModeNonSolid = VK_TRUE;
            f2.features.samplerAnisotropy = VK_TRUE;
            f2.features.multiDrawIndirect = VK_TRUE;
            f2.features.independentBlend = VK_TRUE;
            f2.features.imageCubeArray = VK_TRUE;

            auto& f12 = chain.Feature<VkPhysicalDeviceVulkan12Features>(
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES);
            f12.drawIndirectCount = VK_TRUE;
            f12.shaderInt8 = VK_TRUE;
            f12.shaderFloat16 = VK_TRUE;
            f12.timelineSemaphore = VK_TRUE;
            f12.scalarBlockLayout = VK_TRUE;
            f12.descriptorIndexing = VK_TRUE;
            f12.samplerFilterMinmax = VK_TRUE;
            f12.bufferDeviceAddress = VK_TRUE;
            f12.imagelessFramebuffer = VK_TRUE;
            f12.runtimeDescriptorArray = VK_TRUE;
            f12.descriptorBindingPartiallyBound = VK_TRUE;
            f12.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
            f12.shaderStorageImageArrayNonUniformIndexing = VK_TRUE;
            f12.descriptorBindingUpdateUnusedWhilePending = VK_TRUE;
            f12.shaderInputAttachmentArrayNonUniformIndexing = VK_TRUE;
            f12.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
            f12.descriptorBindingStorageImageUpdateAfterBind = VK_TRUE;
            f12.descriptorBindingUniformBufferUpdateAfterBind = VK_TRUE;

            auto& f13 = chain.Feature<VkPhysicalDeviceVulkan13Features>(
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES);
            f13.maintenance4 = VK_TRUE;
            f13.dynamicRendering = VK_TRUE;
            f13.synchronization2 = VK_TRUE;

            chain.Extension(VK_KHR_FRAGMENT_SHADER_BARYCENTRIC_EXTENSION_NAME);
            chain.Feature<VkPhysicalDeviceFragmentShaderBarycentricFeaturesKHR>(
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_BARYCENTRIC_FEATURES_KHR)
                .fragmentShaderBarycentric = VK_TRUE;

            chain.Feature<VkPhysicalDeviceExtendedDynamicStateFeaturesEXT>(
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT)
                .extendedDynamicState = VK_TRUE;

            chain.Feature<VkPhysicalDeviceExtendedDynamicState2FeaturesEXT>(
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_2_FEATURES_EXT)
                .extendedDynamicState2 = VK_TRUE;

            chain.Extension(VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME);
            chain.Extension(VK_EXT_GRAPHICS_PIPELINE_LIBRARY_EXTENSION_NAME);
            chain.Feature<VkPhysicalDeviceGraphicsPipelineLibraryFeaturesEXT>(
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GRAPHICS_PIPELINE_LIBRARY_FEATURES_EXT)
                .graphicsPipelineLibrary = VK_TRUE;

            chain.Extension(VK_EXT_MUTABLE_DESCRIPTOR_TYPE_EXTENSION_NAME);
            chain.Feature<VkPhysicalDeviceMutableDescriptorTypeFeaturesEXT>(
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MUTABLE_DESCRIPTOR_TYPE_FEATURES_EXT)
                .mutableDescriptorType = VK_TRUE;
        }

        if (config.meshShaders) {
            chain.Extension(VK_EXT_MESH_SHADER_EXTENSION_NAME);
            auto& f = chain.Feature<VkPhysicalDeviceMeshShaderFeaturesEXT>(
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT);

            f.meshShader = true;
            f.meshShaderQueries = true;
            f.multiviewMeshShader = true;
            f.taskShader = true;
        }

        if (config.descriptorBuffers) {
            chain.Extension(VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME);
            auto& db = chain.Feature<VkPhysicalDeviceDescriptorBufferFeaturesEXT>(
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT);
            db.descriptorBuffer = VK_TRUE;
            db.descriptorBufferPushDescriptors = VK_TRUE;
        }

        if (config.rayTracing) {
            chain.Extension(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
            chain.Extension(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
            chain.Extension(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
            chain.Feature<VkPhysicalDeviceRayTracingPipelineFeaturesKHR>(
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR)
                .rayTracingPipeline = VK_TRUE;
            chain.Feature<VkPhysicalDeviceAccelerationStructureFeaturesKHR>(
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR)
                .accelerationStructure = VK_TRUE;

            chain.Extension(VK_KHR_RAY_QUERY_EXTENSION_NAME);
            chain.Feature<VkPhysicalDeviceRayQueryFeaturesKHR>(
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR)
                .rayQuery = VK_TRUE;

            chain.Extension(VK_NV_RAY_TRACING_INVOCATION_REORDER_EXTENSION_NAME);
            chain.Feature<VkPhysicalDeviceRayTracingInvocationReorderFeaturesNV>(
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_INVOCATION_REORDER_FEATURES_NV)
                .rayTracingInvocationReorder = VK_TRUE;

            chain.Extension(VK_KHR_RAY_TRACING_POSITION_FETCH_EXTENSION_NAME);
            chain.Feature<VkPhysicalDeviceRayTracingPositionFetchFeaturesKHR>(
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_POSITION_FETCH_FEATURES_KHR)
                .rayTracingPositionFetch = VK_TRUE;
        }

        auto deviceExtensions = NOVA_ALLOC_STACK(const char*, chain.extensions.size());
        {
            u32 i = 0;
            for (const auto& ext : chain.extensions) {
                deviceExtensions[i++] = ext.c_str();
            }
        }

        auto priorities = NOVA_ALLOC_STACK(float, impl->graphicQueues.size());
        for (u32 i = 0; i < impl->graphicQueues.size(); ++i) {
            priorities[i] = 1.f;
        }

        auto lowPriorities = NOVA_ALLOC_STACK(float, 8);
        for (u32 i = 0; i < 8; ++i) {
            lowPriorities[i] = 0.1f;
        }

        VkCall(vkCreateDevice(impl->gpu, Temp(VkDeviceCreateInfo {
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pNext = chain.Build(),
            .queueCreateInfoCount = 3,
            .pQueueCreateInfos = std::array {
                VkDeviceQueueCreateInfo {
                    .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                    .queueFamilyIndex = impl->graphicQueues.front()->family,
                    .queueCount = u32(impl->graphicQueues.size()),
                    .pQueuePriorities = priorities,
                },
                VkDeviceQueueCreateInfo {
                    .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                    .queueFamilyIndex = impl->transferQueues.front()->family,
                    .queueCount = u32(impl->transferQueues.size()),
                    .pQueuePriorities = lowPriorities,
                },
                VkDeviceQueueCreateInfo {
                    .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                    .queueFamilyIndex = impl->computeQueues.front()->family,
                    .queueCount = u32(impl->computeQueues.size()),
                    .pQueuePriorities = lowPriorities,
                },
            }.data(),
            .enabledExtensionCount = u32(chain.extensions.size()),
            .ppEnabledExtensionNames = deviceExtensions,
        }), impl->pAlloc, &impl->device));

        volkLoadDevice(impl->device);

        vkGetPhysicalDeviceProperties2(impl->gpu, Temp(VkPhysicalDeviceProperties2 {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
            .pNext = &impl->descriptorSizes,
        }));

        // ---- Shared resources ----

        for (u32 i = 0; i < impl->graphicQueues.size(); ++i) {
            vkGetDeviceQueue(impl->device, impl->graphicQueues[i]->family, i, &impl->graphicQueues[i]->handle);
        }

        for (u32 i = 0; i < impl->transferQueues.size(); ++i) {
            vkGetDeviceQueue(impl->device, impl->transferQueues[i]->family, i, &impl->transferQueues[i]->handle);
        }

        for (u32 i = 0; i < impl->computeQueues.size(); ++i) {
            vkGetDeviceQueue(impl->device, impl->computeQueues[i]->family, i, &impl->computeQueues[i]->handle);
        }

        VkCall(vmaCreateAllocator(Temp(VmaAllocatorCreateInfo {
            .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
            .physicalDevice = impl->gpu,
            .device = impl->device,
            .pAllocationCallbacks = impl->pAlloc,
            .pVulkanFunctions = Temp(VmaVulkanFunctions {
                .vkGetInstanceProcAddr = vkGetInstanceProcAddr,
                .vkGetDeviceProcAddr = vkGetDeviceProcAddr,
            }),
            .instance = impl->instance,
            .vulkanApiVersion = VK_API_VERSION_1_3,
        }), &impl->vma));

        {
            // Already rely on a bottomless descriptor pool, so not going to pretend to pass some bogus sizes here.
            // TODO: Check what platforms have bottomless implementations
            // TODO: Runtime profile-based amortized constant pool reallocation?

            constexpr u32 MaxDescriptorPerType = 65'536;
            constexpr auto sizes = std::array {
                VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_SAMPLER,                MaxDescriptorPerType },
                VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MaxDescriptorPerType },
                VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,          MaxDescriptorPerType },
                VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          MaxDescriptorPerType },
                VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,   MaxDescriptorPerType },
                VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,   MaxDescriptorPerType },
                VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         MaxDescriptorPerType },
                VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         MaxDescriptorPerType },
                VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, MaxDescriptorPerType },
                VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, MaxDescriptorPerType },
                VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,       MaxDescriptorPerType },
            };

            VkCall(vkCreateDescriptorPool(impl->device, Temp(VkDescriptorPoolCreateInfo {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT
                    | VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
                .maxSets = u32(MaxDescriptorPerType * sizes.size()),
                .poolSizeCount = u32(sizes.size()),
                .pPoolSizes = sizes.data(),
            }), impl->pAlloc, &impl->descriptorPool));
        }

        VkCall(vkCreatePipelineCache(impl->device, Temp(VkPipelineCacheCreateInfo {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
            .initialDataSize = 0,
        }), impl->pAlloc, &impl->pipelineCache));

        {
            NOVA_LOGEXPR(impl->descriptorSizes.combinedImageSamplerDescriptorSize);
            NOVA_LOGEXPR(impl->descriptorSizes.storageImageDescriptorSize);
            NOVA_LOGEXPR(impl->descriptorSizes.uniformBufferDescriptorSize);
            NOVA_LOGEXPR(impl->descriptorSizes.storageBufferDescriptorSize);
            NOVA_LOGEXPR(impl->descriptorSizes.uniformTexelBufferDescriptorSize);
            NOVA_LOGEXPR(impl->descriptorSizes.storageTexelBufferDescriptorSize);
            NOVA_LOG("Max size: {}", std::max({
                impl->descriptorSizes.combinedImageSamplerDescriptorSize,
                impl->descriptorSizes.storageImageDescriptorSize,
                impl->descriptorSizes.uniformBufferDescriptorSize,
                impl->descriptorSizes.storageBufferDescriptorSize,
                impl->descriptorSizes.uniformTexelBufferDescriptorSize,
                impl->descriptorSizes.storageTexelBufferDescriptorSize,
            }));
        }

        // TODO: Separate "CBV_SRV_UAV" and Sampler heaps into separate layouts
        //         with dynamic number of descriptors
        // TODO: Add acceleration strucvture layout
        // TODO: Remove combined image samplers entirely
        //        - Immutable samplers?

        VkCall(vkCreateDescriptorSetLayout(impl->device, Temp(VkDescriptorSetLayoutCreateInfo {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext = Temp(VkDescriptorSetLayoutBindingFlagsCreateInfo {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
                .pNext = Temp(VkMutableDescriptorTypeCreateInfoEXT {
                    .sType = VK_STRUCTURE_TYPE_MUTABLE_DESCRIPTOR_TYPE_CREATE_INFO_VALVE,
                    .mutableDescriptorTypeListCount = 1,
                    .pMutableDescriptorTypeLists = Temp(VkMutableDescriptorTypeListEXT {
                        .descriptorTypeCount = 6,
                        .pDescriptorTypes = std::array {
                            // VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                            VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
                            VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
                        }.data(),
                    }),
                }),
                .bindingCount = 2,
                .pBindingFlags = std::array<VkDescriptorBindingFlags, 2> {
                    VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT
                        | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT,
                    VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT
                        | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT,
                }.data(),
            }),
            .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
            .bindingCount = 2,
            .pBindings = std::array {
                VkDescriptorSetLayoutBinding {
                    .binding = 0,
                    .descriptorType = VK_DESCRIPTOR_TYPE_MUTABLE_EXT,
                    .descriptorCount = 1024 * 1024,
                    .stageFlags = VK_SHADER_STAGE_ALL,
                },
                VkDescriptorSetLayoutBinding {
                    .binding = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
                    .descriptorCount = 4096,
                    .stageFlags = VK_SHADER_STAGE_ALL,
                },
            }.data(),
        }), impl->pAlloc, &impl->heapLayout));

        VkCall(vkCreateDescriptorSetLayout(impl->device, Temp(VkDescriptorSetLayoutCreateInfo {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR,
            .bindingCount = 1,
            .pBindings = std::array {
                VkDescriptorSetLayoutBinding {
                    .binding = 0,
                    .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
                    .descriptorCount = 1,
                    .stageFlags = VK_SHADER_STAGE_ALL,
                },
            }.data(),
        }), impl->pAlloc, &impl->rtLayout));

        VkCall(vkCreatePipelineLayout(impl->device, Temp(VkPipelineLayoutCreateInfo {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 2,
            .pSetLayouts = std::array {
                impl->heapLayout,
                impl->rtLayout,
            }.data(),
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = Temp(VkPushConstantRange {
                .stageFlags = VK_SHADER_STAGE_ALL,
                .size = 256,
            }),
        }), impl->pAlloc, &impl->pipelineLayout));

        NOVA_LOG("Created!");

        return { impl };
    }

    void Context::Destroy()
    {
        if (!impl) {
            return;
        }

        WaitIdle();

        for (auto& queue : impl->graphicQueues)  { delete queue.impl; }
        for (auto& queue : impl->computeQueues)  { delete queue.impl; }
        for (auto& queue : impl->transferQueues) { delete queue.impl; }

        // Deleted graphics pipeline library stages
        for (auto&[key, pipeline] : impl->vertexInputStages)    { vkDestroyPipeline(impl->device, pipeline, impl->pAlloc); }
        for (auto&[key, pipeline] : impl->preRasterStages)      { vkDestroyPipeline(impl->device, pipeline, impl->pAlloc); }
        for (auto&[key, pipeline] : impl->fragmentShaderStages) { vkDestroyPipeline(impl->device, pipeline, impl->pAlloc); }
        for (auto&[key, pipeline] : impl->fragmentOutputStages) { vkDestroyPipeline(impl->device, pipeline, impl->pAlloc); }
        for (auto&[key, pipeline] : impl->graphicsPipelineSets) { vkDestroyPipeline(impl->device, pipeline, impl->pAlloc); }
        for (auto&[key, pipeline] : impl->computePipelines)     { vkDestroyPipeline(impl->device, pipeline, impl->pAlloc); }

        // Destroy context vk objects
        vkDestroyPipelineLayout(impl->device, impl->pipelineLayout, impl->pAlloc);
        vkDestroyDescriptorSetLayout(impl->device, impl->rtLayout, impl->pAlloc);
        vkDestroyDescriptorSetLayout(impl->device, impl->heapLayout, impl->pAlloc);
        vkDestroyPipelineCache(impl->device, impl->pipelineCache, impl->pAlloc);
        vkDestroyDescriptorPool(impl->device, impl->descriptorPool, impl->pAlloc);
        vmaDestroyAllocator(impl->vma);
        vkDestroyDevice(impl->device, impl->pAlloc);
        if (impl->debugMessenger) {
            vkDestroyDebugUtilsMessengerEXT(impl->instance, impl->debugMessenger, impl->pAlloc);
        }
        vkDestroyInstance(impl->instance, impl->pAlloc);

        NOVA_LOG("~Context(Allocations = {})", rhi::stats::AllocationCount.load());

        delete impl;
        impl = nullptr;
    }

    void Context::WaitIdle() const
    {
        vkDeviceWaitIdle(impl->device);
    }

    const ContextConfig& Context::GetConfig() const
    {
        return impl->config;
    }
}