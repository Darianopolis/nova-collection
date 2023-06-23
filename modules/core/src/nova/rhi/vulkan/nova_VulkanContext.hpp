#include <nova/rhi/nova_RHI.hpp>

#include <nova/core/nova_Registry.hpp>

namespace nova
{
    enum class UID : u64 {
        Invalid = 0,
    };

    struct VulkanQueue
    {
        VkQueue handle = {};
        u32     family = UINT32_MAX;
    };

    struct VulkanFence
    {
        VkSemaphore semaphore = {};
        u64             value = 0;
    };

    struct VulkanCommandPool
    {
        Queue     queue = {};

        VkCommandPool               pool = {};
        std::vector<CommandList> lists = {};
        u32                        index = 0;
    };

    struct VulkanCommandState
    {
        struct ImageState
        {
            VkImageLayout        layout = VK_IMAGE_LAYOUT_UNDEFINED;
            VkPipelineStageFlags2 stage = VK_PIPELINE_STAGE_2_NONE;
            VkAccessFlags2       access = 0;

            u64 version = 0;
        };

        ankerl::unordered_dense::map<VkImage, ImageState> imageStates;

        std::vector<Format> colorAttachmentsFormats;
        Format                depthAttachmentFormat = nova::Format::Undefined;
        Format              stencilAttachmentFormat = nova::Format::Undefined;
        Vec2U                       renderingExtent;
    };

    struct VulkanCommandList
    {
        CommandPool       pool = {};
        CommandState     state = {};
        VkCommandBuffer buffer = {};
    };

    struct VulkanSwapchain
    {
        VkSurfaceKHR          surface = {};
        VkSwapchainKHR      swapchain = {};
        VkSurfaceFormatKHR     format = { VK_FORMAT_UNDEFINED, VK_COLORSPACE_SRGB_NONLINEAR_KHR };
        VkImageUsageFlags       usage = 0;
        VkPresentModeKHR  presentMode = VK_PRESENT_MODE_FIFO_KHR;
        std::vector<Texture> textures = {};
        uint32_t                index = UINT32_MAX;
        VkExtent2D             extent = { 0, 0 };
        bool                  invalid = false;

        std::vector<VkSemaphore> semaphores = {};
        u32                  semaphoreIndex = 0;
    };

    struct VulkanPipelineLayout
    {
        UID id = UID::Invalid;

        VkPipelineLayout layout;

        std::vector<DescriptorSetLayout> setLayouts;
        std::vector<VkPushConstantRange> ranges;
    };

    struct VulkanShader
    {
        UID id = UID::Invalid;

        VkShaderModule       handle = {};
        VkShaderStageFlagBits stage = {};

        constexpr static const char* EntryPoint = "main";
    };

    struct VulkanBuffer
    {
        VkBuffer          buffer = {};
        VmaAllocation allocation = {};
        VkDeviceSize        size = 0ull;
        VkDeviceAddress  address = 0ull;
        b8*               mapped = nullptr;
        BufferFlags        flags = BufferFlags::None;
        VkBufferUsageFlags usage = {};
    };

    struct VulkanTexture
    {
        VkImage             image = {};
        VmaAllocation  allocation = {};
        VkImageView          view = {};
        VkImageUsageFlags   usage = {};
        VkFormat           format = VK_FORMAT_UNDEFINED;
        VkImageAspectFlags aspect = VK_IMAGE_ASPECT_NONE;

        Vec3U extent = {};
        u32     mips = 0;
        u32   layers = 0;
    };

// -----------------------------------------------------------------------------

#define NOVA_DEFINE_WYHASH_EQUALITY(type)                    \
    bool operator==(const type& other) const                 \
    {                                                        \
        return std::memcmp(this, &other, sizeof(type)) == 0; \
    }

#define NOVA_DEFINE_WYHASH_FOR(type)                          \
    template<> struct ankerl::unordered_dense::hash<type> {   \
        using is_avalanching = void;                          \
        uint64_t operator()(const type& key) const noexcept { \
            return detail::wyhash::hash(&key, sizeof(key));   \
        }                                                     \
    }

    struct GraphicsPipelineVertexInputStageKey
    {
        VkPrimitiveTopology topology;

        NOVA_DEFINE_WYHASH_EQUALITY(GraphicsPipelineVertexInputStageKey)
    };

    struct GraphicsPipelinePreRasterizationStageKey
    {
        std::array<UID, 4> shaders;
        UID                 layout;
        VkPolygonMode     polyMode;

        NOVA_DEFINE_WYHASH_EQUALITY(GraphicsPipelinePreRasterizationStageKey)
    };

    struct GraphicsPipelineFragmentShaderStageKey
    {
        UID shader;
        UID layout;

        NOVA_DEFINE_WYHASH_EQUALITY(GraphicsPipelineFragmentShaderStageKey)
    };

    struct GraphicsPipelineFragmentOutputStageKey
    {
        std::array<VkFormat, 8> colorAttachments;
        VkFormat                 depthAttachment;
        VkFormat               stencilAttachment;

        VkBool32 blendEnable;

        NOVA_DEFINE_WYHASH_EQUALITY(GraphicsPipelineFragmentOutputStageKey)
    };

    struct GraphicsPipelineLibrarySetKey
    {
        std::array<VkPipeline, 4> stages;

        NOVA_DEFINE_WYHASH_EQUALITY(GraphicsPipelineLibrarySetKey)
    };
}

NOVA_DEFINE_WYHASH_FOR(nova::GraphicsPipelineVertexInputStageKey);
NOVA_DEFINE_WYHASH_FOR(nova::GraphicsPipelinePreRasterizationStageKey);
NOVA_DEFINE_WYHASH_FOR(nova::GraphicsPipelineFragmentShaderStageKey);
NOVA_DEFINE_WYHASH_FOR(nova::GraphicsPipelineFragmentOutputStageKey);
NOVA_DEFINE_WYHASH_FOR(nova::GraphicsPipelineLibrarySetKey);

namespace nova
{

// -----------------------------------------------------------------------------

    struct VulkanContext : Context
    {
        ContextConfig config;

        VkInstance  instance = {};
        VkPhysicalDevice gpu = {};
        VkDevice      device = {};
        VmaAllocator     vma = {};

        VkDebugUtilsMessengerEXT debugMessenger = {};

    public:
        VkDescriptorPool descriptorPool = {};

        Queue graphics = {};

    public:
        VkPhysicalDeviceRayTracingPipelinePropertiesKHR rayTracingPipelineProperties = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR,
        };

        VkPhysicalDeviceAccelerationStructurePropertiesKHR accelStructureProperties = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR,
            .pNext = &rayTracingPipelineProperties,
        };

        VkPhysicalDeviceDescriptorBufferPropertiesEXT descriptorSizes = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_PROPERTIES_EXT,
            .pNext = &accelStructureProperties,
        };

    public:
        std::atomic_uint64_t nextUID = 1;
        UID GetUID() noexcept { return UID(nextUID++); };

        ankerl::unordered_dense::map<GraphicsPipelineVertexInputStageKey, VkPipeline>       vertexInputStages;
        ankerl::unordered_dense::map<GraphicsPipelinePreRasterizationStageKey, VkPipeline>    preRasterStages;
        ankerl::unordered_dense::map<GraphicsPipelineFragmentShaderStageKey, VkPipeline> fragmentShaderStages;
        ankerl::unordered_dense::map<GraphicsPipelineFragmentOutputStageKey, VkPipeline> fragmentOutputStages;
        ankerl::unordered_dense::map<GraphicsPipelineLibrarySetKey, VkPipeline>          graphicsPipelineSets;

        VkPipelineCache pipelineCache = {};

    public:
        static std::atomic_int64_t    AllocationCount;
        static std::atomic_int64_t NewAllocationCount;

        VkAllocationCallbacks alloc = {
            .pfnAllocation = +[](void*, size_t size, size_t align, [[maybe_unused]] VkSystemAllocationScope scope) {
                void* ptr = mi_malloc_aligned(size, align);
                if (ptr)
                {
                    ++AllocationCount;
                    ++NewAllocationCount;
#ifdef NOVA_NOISY_VULKAN_ALLOCATIONS
                    NOVA_LOG(" --\n{}", std::stacktrace::current());
                    NOVA_LOG("Allocating size = {}, align = {}, scope = {}, ptr = {}", size, align, int(scope), ptr);
#endif
                }
                return ptr;
            },
            .pfnReallocation = +[](void*, void* orig, size_t size, size_t align, VkSystemAllocationScope) {
                void* ptr = mi_realloc_aligned(orig, size, align);
#ifdef NOVA_NOISY_VULKAN_ALLOCATIONS
                NOVA_LOG("Reallocated, size = {}, align = {}, ptr = {} -> {}", size, align, orig, ptr);
#endif
                return ptr;
            },
            .pfnFree = +[](void*, void* ptr) {
                if (ptr)
                {
                    --AllocationCount;
#ifdef NOVA_NOISY_VULKAN_ALLOCATIONS
                    NOVA_LOG("Freeing ptr = {}", ptr);
                    NOVA_LOG("    Allocations - :: {}", AllocationCount.load());
#endif
                }
                mi_free(ptr);
            },
            .pfnInternalAllocation = +[](void*, size_t size, VkInternalAllocationType type, VkSystemAllocationScope) {
                NOVA_LOG("Internal allocation of size {}, type = {}", size, int(type));
            },
            .pfnInternalFree = +[](void*, size_t size, VkInternalAllocationType type, VkSystemAllocationScope) {
                NOVA_LOG("Internal free of size {}, type = {}", size, int(type));
            },
        };
        VkAllocationCallbacks* pAlloc = &alloc;

    public:
        VulkanContext(const ContextConfig& config);
        ~VulkanContext();

        void WaitIdle() override;

#define NOVA_ADD_VULKAN_REGISTRY(type, name) \
    Registry<Vulkan##type, type> name;       \
    Vulkan##type& Get(type id) { return name.Get(id); }

// -----------------------------------------------------------------------------
//                                 Queue
// -----------------------------------------------------------------------------

        NOVA_ADD_VULKAN_REGISTRY(Queue, queues)

        Queue Queue_Get(QueueFlags flags) override;
        void  Queue_Submit(Queue, Span<CommandList> commandLists, Span<Fence> waits, Span<Fence> signals) override;
        bool  Queue_Acquire(Queue, Span<Swapchain> swapchains, Span<Fence> signals) override;
        void  Queue_Present(Queue, Span<Swapchain> swapchains, Span<Fence> waits, bool hostWait = false) override;

// -----------------------------------------------------------------------------
//                                 Fence
// -----------------------------------------------------------------------------

        NOVA_ADD_VULKAN_REGISTRY(Fence, fences)

        Fence Fence_Create() override;
        void  Destroy(Fence) override;
        void  Fence_Wait(Fence, u64 waitValue = 0ull) override;
        u64   Fence_Advance(Fence) override;
        void  Fence_Signal(Fence, u64 signalValue = 0ull) override;
        u64   Fence_GetPendingValue(Fence) override;

// -----------------------------------------------------------------------------
//                                Commands
// -----------------------------------------------------------------------------

        NOVA_ADD_VULKAN_REGISTRY(CommandPool, commandPools)
        NOVA_ADD_VULKAN_REGISTRY(CommandState, commandStates)
        NOVA_ADD_VULKAN_REGISTRY(CommandList, commandLists)

        CommandPool Commands_CreatePool(Queue queue) override;
        void        Destroy(CommandPool) override;
        CommandList Commands_Begin(CommandPool pool, CommandState state) override;
        void        Commands_Reset(CommandPool pool) override;

        CommandState Commands_CreateState() override;
        void         Commands_SetState(CommandState state, Texture texture,
            VkImageLayout layout, VkPipelineStageFlags2 stages, VkAccessFlags2 access) override;

// -----------------------------------------------------------------------------
//                                Swapchain
// -----------------------------------------------------------------------------

        NOVA_ADD_VULKAN_REGISTRY(Swapchain, swapchains)

        Swapchain Swapchain_Create(void* window, TextureUsage usage, PresentMode presentMode) override;
        void      Destroy(Swapchain) override;
        Texture   Swapchain_GetCurrent(Swapchain) override;
        Vec2U     Swapchain_GetExtent(Swapchain) override;
        Format    Swapchain_GetFormat(Swapchain) override;

        void Cmd_Present(CommandList, Swapchain swapchain) override;

// -----------------------------------------------------------------------------
//                                  Shader
// -----------------------------------------------------------------------------

        NOVA_ADD_VULKAN_REGISTRY(Shader, shaders)

        Shader Shader_Create(ShaderStage stage, const std::string& filename, const std::string& sourceCode = {}) override;
        void   Destroy(Shader) override;

// -----------------------------------------------------------------------------
//                             Pipeline Layout
// -----------------------------------------------------------------------------

        NOVA_ADD_VULKAN_REGISTRY(PipelineLayout, pipelineLayouts)

        PipelineLayout Pipelines_CreateLayout() override;
        void           Destroy(PipelineLayout) override;

        void Cmd_SetGraphicsState(CommandList, PipelineLayout layout, Span<Shader> shaders, const PipelineState& state) override;

// -----------------------------------------------------------------------------
//                                Drawing
// -----------------------------------------------------------------------------

        void Cmd_BeginRendering(CommandList, Span<Texture> colorAttachments, Texture depthAttachment = {}, Texture stencilAttachment = {}) override;
        void Cmd_EndRendering(CommandList) override;
        void Cmd_Draw(CommandList, u32 vertices, u32 instances, u32 firstVertex, u32 firstInstance) override;

// -----------------------------------------------------------------------------
//                                 Buffer
// -----------------------------------------------------------------------------

        NOVA_ADD_VULKAN_REGISTRY(Buffer, buffers)

        Buffer Buffer_Create(u64 size, BufferUsage usage, BufferFlags flags = {}) override;
        void   Destroy(Buffer) override;
        void   Buffer_Resize(Buffer, u64 size) override;
        u64    Buffer_GetSize(Buffer) override;
        b8*    Buffer_GetMapped(Buffer) override;
        u64    Buffer_GetAddress(Buffer) override;
        void*  BufferImpl_Get(Buffer, u64 index, u64 offset, usz stride) override;
        void   BufferImpl_Set(Buffer, const void* data, usz count, u64 index, u64 offset, usz stride) override;

// -----------------------------------------------------------------------------
//                                 Texture
// -----------------------------------------------------------------------------

        NOVA_ADD_VULKAN_REGISTRY(Texture, textures)

        Texture Texture_Create(Vec3U size, TextureUsage usage, Format format, TextureFlags flags = {}) override;
        void    Destroy(Texture) override;
        Vec3U   Texture_GetExtent(Texture) override;
        Format  Texture_GetFormat(Texture) override;

        void Cmd_Transition(CommandList, Texture texture, VkImageLayout newLayout, VkPipelineStageFlags2 newStages, VkAccessFlags2 newAccess) override;
        void Cmd_Clear(CommandList, Texture texture, Vec4 color) override;
    };
}