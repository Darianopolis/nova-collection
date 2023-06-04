#pragma once

#include <nova/core/nova_Core.hpp>
#include <nova/core/nova_ImplHandle.hpp>

// #define NOVA_NOISY_VULKAN_ALLOCATIONS

namespace nova
{
    inline std::atomic<u64> submitting = 0;
    inline std::atomic<u64> adapting1 = 0;
    inline std::atomic<u64> adapting2 = 0;
    inline std::atomic<u64> presenting = 0;

    inline
    void VkCall(VkResult res)
    {
        if (res != VK_SUCCESS)
            NOVA_THROW("Error: {}", int(res));
    }

    template<class Container, class Fn, class ... Args>
    void VkQuery(Container&& container, Fn&& fn, Args&& ... args)
    {
        u32 count;
        fn(std::forward<Args>(args)..., &count, nullptr);
        container.resize(count);
        fn(std::forward<Args>(args)..., &count, container.data());
    }

// -----------------------------------------------------------------------------

    #define NOVA_DECL_DEVICE_PROC(name)  inline PFN_##name name
    #define NOVA_LOAD_DEVICE_PROC(name, device) ::nova::name = (PFN_##name)vkGetDeviceProcAddr(device, #name);\
        NOVA_LOG("Loaded fn [" #name "] - {}", (void*)name)

// -----------------------------------------------------------------------------

    struct Buffer;
    struct CommandList;
    struct CommandPool;
    struct Context;
    struct DescriptorSetLayout;
    struct Fence;
    struct PipelineLayout;
    struct Queue;
    struct ResourceTracker;
    struct Sampler;
    struct Shader;
    struct Swapchain;
    struct Texture;
    struct AccelerationStructure;
    struct RayTracingPipeline;

    struct ContextImpl;
    struct BufferImpl;
    struct CommandListImpl;
    struct CommandPoolImpl;
    struct DescriptorLayoutImpl;
    struct FenceImpl;
    struct PipelineLayoutImpl;
    struct QueueImpl;
    struct ResourceTrackerImpl;
    struct SamplerImpl;
    struct ShaderImpl;
    struct SwapchainImpl;
    struct TextureImpl;
    struct AccelerationStructureImpl;
    struct RayTracingPipelineImpl;

// -----------------------------------------------------------------------------

    enum class BufferFlags
    {
        None,
        Addressable  = 1 << 0,
        DeviceLocal  = 1 << 1 | Addressable,
        Mappable     = 1 << 2,
        CreateMapped = 1 << 3 | Mappable,
    };
    NOVA_DECORATE_FLAG_ENUM(BufferFlags)

    enum class BufferUsage : u64
    {
        TransferSrc = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        TransferDst = VK_BUFFER_USAGE_TRANSFER_DST_BIT,

        Uniform = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        Storage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,

        Index = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        Vertex = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,

        Indirect = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,

        ShaderBindingTable = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR,

        AccelBuild = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        AccelStorage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR,

        DescriptorSamplers = VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT,
        DescriptorResources = VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT,
    };
    NOVA_DECORATE_FLAG_ENUM(BufferUsage)

    enum class TextureFlags
    {
        Array = 1 << 0,
        Mips  = 1 << 1,
    };
    NOVA_DECORATE_FLAG_ENUM(TextureFlags)

    enum class TextureUsage
    {
        TransferSrc        = VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        TransferDst        = VK_IMAGE_USAGE_TRANSFER_DST_BIT,

        Sampled            = VK_IMAGE_USAGE_SAMPLED_BIT,
        Storage            = VK_IMAGE_USAGE_STORAGE_BIT,

        ColorAttach        = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        DepthStencilAttach = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
    };
    NOVA_DECORATE_FLAG_ENUM(TextureUsage)

    enum class Format
    {
        Undefined = VK_FORMAT_UNDEFINED,

        RGBA8U = VK_FORMAT_R8G8B8A8_UNORM,
        RGBA16F = VK_FORMAT_R16G16B16A16_SFLOAT,
        RGBA32F = VK_FORMAT_R32G32B32A32_SFLOAT,

        RGB32F = VK_FORMAT_R32G32B32_SFLOAT,

        R8U = VK_FORMAT_R8_UNORM,
        R32F = VK_FORMAT_R32_SFLOAT,

        R8UInt = VK_FORMAT_R8_UINT,
        R16UInt = VK_FORMAT_R16_UINT,
        R32UInt = VK_FORMAT_R32_UINT,

        D24U_X8 = VK_FORMAT_X8_D24_UNORM_PACK32,
        D24U_S8 = VK_FORMAT_D24_UNORM_S8_UINT,
    };

    enum class IndexType
    {
        U16 = VK_INDEX_TYPE_UINT16,
        U32 = VK_INDEX_TYPE_UINT32,
        U8 = VK_INDEX_TYPE_UINT8_EXT,
    };

    enum class Filter : u32
    {
        Linear = VK_FILTER_LINEAR,
        Nearest = VK_FILTER_NEAREST,
    };

    enum class AddressMode : u32
    {
        Repeat = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        RepeatMirrored = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
        Edge = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        Border = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
    };

    enum class BorderColor : u32
    {
        TransparentBlack = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
        Black = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK,
        White = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
    };

    enum class ShaderStage : uint16_t
    {
        None = 0,

        Vertex = VK_SHADER_STAGE_VERTEX_BIT,
        TessControl = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
        TessEval = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
        Geometry = VK_SHADER_STAGE_GEOMETRY_BIT,
        Fragment = VK_SHADER_STAGE_FRAGMENT_BIT,

        Compute = VK_SHADER_STAGE_COMPUTE_BIT,

        RayGen = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
        AnyHit = VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
        ClosestHit = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
        Miss = VK_SHADER_STAGE_MISS_BIT_KHR,
        Intersection = VK_SHADER_STAGE_INTERSECTION_BIT_KHR,
    };
    NOVA_DECORATE_FLAG_ENUM(ShaderStage)

    enum class PresentMode : u32
    {
        Immediate   = VK_PRESENT_MODE_IMMEDIATE_KHR,
        Mailbox     = VK_PRESENT_MODE_MAILBOX_KHR,
        Fifo        = VK_PRESENT_MODE_FIFO_KHR,
        FifoRelaxed = VK_PRESENT_MODE_FIFO_RELAXED_KHR,
    };

    enum class ResourceState
    {
        GeneralImage,
        Present,
    };

    enum class BindPoint : u32
    {
        // TODO: Support transfer
        Graphics = VK_PIPELINE_BIND_POINT_GRAPHICS,
        Compute = VK_PIPELINE_BIND_POINT_COMPUTE,
        RayTracing = VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
    };

    enum class AccelerationStructureType : u32
    {
        BottomLevel = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
        TopLevel = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
    };

    enum class AccelerationStructureFlags : u32
    {
        PreferFastTrace = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        PreferFastBuild = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR,
        AllowDataAccess = VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_DATA_ACCESS_KHR,
        AllowCompaction = VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR,
    };
    NOVA_DECORATE_FLAG_ENUM(AccelerationStructureFlags)

    enum class GeometryInstanceFlags : u32
    {
        TriangleCullClockwise        = 1 << 0,
        TriangleCullCounterClockwise = 1 << 1,
        InstanceForceOpaque          = 1 << 2,
    };
    NOVA_DECORATE_FLAG_ENUM(GeometryInstanceFlags)

    enum class DescriptorType : u32
    {
        SampledTexture = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        StorageTexture = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        Uniform = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        Storage = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        AccelerationStructure = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
    };

    enum class CompareOp
    {
        Never          = VK_COMPARE_OP_NEVER,
        Less           = VK_COMPARE_OP_LESS,
        Equal          = VK_COMPARE_OP_EQUAL,
        LessOrEqual    = VK_COMPARE_OP_LESS_OR_EQUAL,
        Greater        = VK_COMPARE_OP_GREATER,
        NotEqual       = VK_COMPARE_OP_NOT_EQUAL,
        GreaterOrEqual = VK_COMPARE_OP_GREATER_OR_EQUAL,
        Always         = VK_COMPARE_OP_ALWAYS,
    };

    enum class CullMode
    {
        None  = VK_CULL_MODE_NONE,
        Front = VK_CULL_MODE_FRONT_BIT,
        Back  = VK_CULL_MODE_BACK_BIT,
    };
    NOVA_DECORATE_FLAG_ENUM(CullMode);

    enum class FrontFace
    {
        CounterClockwise = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        Clockwise = VK_FRONT_FACE_CLOCKWISE,
    };

    enum class PolygonMode
    {
        Fill = VK_POLYGON_MODE_FILL,
        Line = VK_POLYGON_MODE_LINE,
        Point = VK_POLYGON_MODE_POINT
    };

    enum class Topology
    {
        Points = VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
        Lines = VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
        LineStrip = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,
        Triangles = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        TriangleStrip = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
        TriangleFan = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN,
        Patches = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST,
    };

    enum class CommandListType
    {
        Primary,
        Secondary,
    };

    enum class QueueFlags
    {
        Graphics = VK_QUEUE_GRAPHICS_BIT,
        Compute  = VK_QUEUE_COMPUTE_BIT,
        Transfer = VK_QUEUE_TRANSFER_BIT
    };
    NOVA_DECORATE_FLAG_ENUM(QueueFlags)

// -----------------------------------------------------------------------------

    struct BufferImpl : ImplBase
    {
        ContextImpl* context = {};

        VkBuffer          buffer = {};
        VmaAllocation allocation = {};
        VkDeviceSize        size = 0ull;
        VkDeviceAddress  address = 0ull;
        b8*               mapped = nullptr;
        BufferFlags        flags = BufferFlags::None;
        VkBufferUsageFlags usage = {};

    public:
        ~BufferImpl();
    };

    struct Buffer : ImplHandle<BufferImpl>
    {
        Buffer() = default;
        Buffer(Context context, u64 size, BufferUsage usage, BufferFlags flags = {});

    public:
        void Resize(u64 size) const;

        b8* GetMapped() const noexcept;
        u64 GetAddress() const noexcept;

        template<class T>
        T& Get(u64 index, u64 offset = 0) const noexcept
        {
            return reinterpret_cast<T*>(GetImpl()->mapped + offset)[index];
        }

        template<class T>
        void Set(Span<T> elements, u64 index = 0, u64 offset = 0) const noexcept
        {
            std::memcpy(reinterpret_cast<T*>(GetImpl()->mapped + offset) + index, &elements[0], elements.size() * sizeof(T));
        }
    };

// -----------------------------------------------------------------------------

    struct SamplerImpl : ImplBase
    {
        ContextImpl* context = {};

        VkSampler sampler = {};

    public:
        ~SamplerImpl();
    };

    struct Sampler : ImplHandle<SamplerImpl>
    {
        Sampler() = default;
        Sampler(Context context, Filter filter, AddressMode addressMode, BorderColor color, f32 anistropy = 0.f);
    };

    struct TextureImpl : ImplBase
    {
        ContextImpl* context = {};

        VkImage            image = {};
        VmaAllocation allocation = {};
        VkImageView         view = {};

        VkFormat           format = VK_FORMAT_UNDEFINED;
        VkImageAspectFlags aspect = VK_IMAGE_ASPECT_NONE;

        Vec3U extent = {};
        u32     mips = 0;
        u32   layers = 0;

    public:
        ~TextureImpl();
    };

    struct Texture : ImplHandle<TextureImpl>
    {
        Texture() = default;
        Texture(Context context, Vec3U size, TextureUsage usage, Format format, TextureFlags flags = {});

        Vec3U GetExtent() const noexcept;
    };

// -----------------------------------------------------------------------------

    struct ShaderImpl : ImplBase
    {
        ContextImpl* context = {};

        VkShaderStageFlagBits stage = VkShaderStageFlagBits(0);
        VkShaderEXT          shader = {};
        VkShaderModule       module = {};

        constexpr static const char* EntryPoint = "main";

    public:
        ~ShaderImpl();
    };

    struct Shader : ImplHandle<ShaderImpl>
    {
        Shader() = default;
        Shader(Context context, ShaderStage stage, ShaderStage nextStage,
            const std::string& filename, const std::string& sourceCode,
            PipelineLayout layout);

    public:
        VkPipelineShaderStageCreateInfo GetStageInfo() const noexcept;
    };

// -----------------------------------------------------------------------------

    struct SurfaceImpl : ImplBase
    {
        ContextImpl* context = {};

        VkSurfaceKHR surface = {};
    public:
        ~SurfaceImpl();
    };

    struct Surface : ImplHandle<SurfaceImpl>
    {
        Surface() = default;
        Surface(Context context, void* handle);
    };

// -----------------------------------------------------------------------------

    struct SwapchainImpl : ImplBase
    {
        ContextImpl* context = {};

        VkSurfaceKHR           surface = nullptr;
        VkSwapchainKHR       swapchain = nullptr;
        VkSurfaceFormatKHR      format = { VK_FORMAT_UNDEFINED, VK_COLORSPACE_SRGB_NONLINEAR_KHR };
        VkImageUsageFlags        usage = 0;
        VkPresentModeKHR   presentMode = VK_PRESENT_MODE_FIFO_KHR;
        std::vector<Texture>  textures = {};
        uint32_t                 index = UINT32_MAX;
        VkExtent2D              extent = { 0, 0 };
        bool                   invalid = false;

        std::vector<VkSemaphore> semaphores = {};
        u32                  semaphoreIndex = 0;

    public:
        ~SwapchainImpl();
    };

    struct Swapchain : ImplHandle<SwapchainImpl>
    {
        Swapchain() = default;
        Swapchain(Context context, Surface surface, TextureUsage usage, PresentMode presentMode);

    public:
        Texture GetCurrent() const noexcept;
        Vec2U GetExtent() const noexcept;
    };

// -----------------------------------------------------------------------------

    struct QueueImpl : ImplBase
    {
        ContextImpl* context = {};

        VkQueue handle = {};
        u32     family = UINT32_MAX;
    };

    struct Queue : ImplHandle<QueueImpl>
    {
        Queue() = default;
        Queue(Context context, VkQueue queue, u32 family);

    public:
        void Submit(Span<CommandList> commandLists, Span<Fence> waits, Span<Fence> signals) const;
        bool Acquire(Span<Swapchain> swapchains, Span<Fence> signals) const;

        // Present a set of swapchains, waiting on a number of fences.
        // If any wait dependency includes a wait-before-signal operation
        // (including indirectly) then hostWait must be set to true, as WSI
        // operations are incompatible with wait-before-signal.
        void Present(Span<Swapchain> swapchains, Span<Fence> waits, bool hostWait = false) const;
    };

// -----------------------------------------------------------------------------

    struct FenceImpl : ImplBase
    {
        ContextImpl*  context = {};

        VkSemaphore semaphore = {};
        u64             value = 0;

    public:
        ~FenceImpl();
    };

    struct Fence : ImplHandle<FenceImpl>
    {
        Fence() = default;
        Fence(Context context);

    public:
        void Wait(u64 waitValue = 0ull) const;
        u64 Advance() const noexcept;
        void Signal(u64 signalValue = 0ull) const;
    };

// -----------------------------------------------------------------------------

    struct DescriptorBinding
    {
        DescriptorType type;
        u32           count = 1;
    };

    struct DescriptorSetBindingOffset
    {
        u32 buffer;
        u64 offset = {};
    };

    struct DescriptorSetLayoutImpl : ImplBase
    {
        ContextImpl* context = {};

        VkDescriptorSetLayout layout = {};
        u64                     size = 0;
        std::vector<u64>     offsets = {};

    public:
        ~DescriptorSetLayoutImpl();
    };

    struct DescriptorSetLayout : ImplHandle<DescriptorSetLayoutImpl>
    {
        DescriptorSetLayout() = default;
        DescriptorSetLayout(Context context, Span<DescriptorBinding> bindings, bool pushDescriptor = false);

    public:
        u64 GetSize() const noexcept;
        void WriteSampledTexture(void* dst, u32 binding, Texture texture, Sampler sampler, u32 arrayIndex = 0) const noexcept;
    };

// -----------------------------------------------------------------------------

    struct PushConstantRange
    {
        ShaderStage stages;
        u32           size;
        u32         offset = 0;
    };

    struct PipelineLayoutImpl : ImplBase
    {
        ContextImpl* context = {};

        VkPipelineLayout layout = {};

        // TODO: Pipeline layout used in multiple bind points?
        BindPoint bindPoint = {};

        std::vector<VkPushConstantRange> ranges;
        std::vector<VkDescriptorSetLayout> sets;

    public:
        ~PipelineLayoutImpl();
    };

    struct PipelineLayout : ImplHandle<PipelineLayoutImpl>
    {
        PipelineLayout() = default;
        PipelineLayout(Context context,
            Span<PushConstantRange> pushConstantRanges,
            Span<DescriptorSetLayout> descriptorLayouts,
            BindPoint bindPoint);
    };

// -----------------------------------------------------------------------------

    struct ResourceTrackerImpl : ImplBase
    {
        struct ImageState
        {
            VkImageLayout        layout = VK_IMAGE_LAYOUT_UNDEFINED;
            VkPipelineStageFlags2 stage = VK_PIPELINE_STAGE_2_NONE;
            VkAccessFlags2       access = 0;

            u64 version = 0;
        };

    public:
        ContextImpl* context = {};

        u64                                                   version = 0;
        ankerl::unordered_dense::map<VkImage, ImageState> imageStates;
        std::vector<VkImage>                                clearList;

    public:
        ImageState& Get(Texture texture) noexcept;
    };

    struct ResourceTracker : ImplHandle<ResourceTrackerImpl>
    {
        ResourceTracker() = default;
        ResourceTracker(Context context);

    public:
        void Clear(u32 maxAge) const noexcept;

        void Reset(Texture texture) const noexcept;
        void Persist(Texture texture) const noexcept;
        void Set(Texture texture, VkImageLayout layout, VkPipelineStageFlags2 stage, VkAccessFlags2 access) const noexcept;
    };

// -----------------------------------------------------------------------------

    struct AccelerationStructureBuilderImpl : ImplBase
    {
        ContextImpl* context = {};

        VkAccelerationStructureTypeKHR        type = {};
        VkBuildAccelerationStructureFlagsKHR flags = {};

        u64         buildSize = 0;
        u64  buildScratchSize = 0;
        u64 updateScratchSize = 0;

        std::vector<VkAccelerationStructureGeometryKHR>   geometries;
        std::vector<u32>                             primitiveCounts;
        std::vector<VkAccelerationStructureBuildRangeInfoKHR> ranges;

        u32 geometryCount = 0;
        u32 firstGeometry = 0;
        bool sizeDirty = false;

        VkQueryPool queryPool = {};

    public:
        ~AccelerationStructureBuilderImpl();

        void EnsureGeometries(u32 geometryIndex);
        void EnsureSizes();
    };

    struct AccelerationStructureBuilder : ImplHandle<AccelerationStructureBuilderImpl>
    {
        AccelerationStructureBuilder() = default;
        AccelerationStructureBuilder(Context context);

    public:
        void SetInstances(u32 geometryIndex, u64 deviceAddress, u32 count) const;
        void SetTriangles(u32 geometryIndex,
            u64 vertexAddress, Format vertexFormat, u32 vertexStride, u32 maxVertex,
            u64 indexAddress, IndexType indexFormat, u32 triangleCount) const;

        void Prepare(nova::AccelerationStructureType type, nova::AccelerationStructureFlags flags,
            u32 geometryCount, u32 firstGeometry = 0u) const;

        u64 GetInstanceSize() const;
        void WriteInstance(
            void* bufferAddress, u32 index,
            AccelerationStructure& structure,
            const Mat4& matrix,
            u32 customIndex, u8 mask,
            u32 sbtOffset, GeometryInstanceFlags flags) const;

        u64 GetBuildSize() const;
        u64 GetBuildScratchSize() const;
        u64 GetUpdateScratchSize() const;
        u64 GetCompactSize() const;
    };

// -----------------------------------------------------------------------------

    struct AccelerationStructureImpl : ImplBase
    {
        ContextImpl* context = {};

        VkAccelerationStructureKHR structure = {};
        u64                          address = {};
        VkAccelerationStructureTypeKHR  type = {};

        Buffer buffer = {};

    public:
        ~AccelerationStructureImpl();
    };

    struct AccelerationStructure : ImplHandle<AccelerationStructureImpl>
    {
        AccelerationStructure() = default;
        AccelerationStructure(Context context, usz size, AccelerationStructureType type);

    public:
        u64 GetAddress() const noexcept;
    };

// -----------------------------------------------------------------------------

    struct HitShaderGroup
    {
        Shader closestHitShader = {};
        Shader anyHitShader = {};
        Shader intersectionShader = {};
    };

    struct RayTracingPipelineImpl : ImplBase
    {
        ContextImpl* context = {};

        VkPipeline pipeline = {};
        Buffer    sbtBuffer = {};

        VkStridedDeviceAddressRegionKHR rayGenRegion = {};
        VkStridedDeviceAddressRegionKHR rayMissRegion = {};
        VkStridedDeviceAddressRegionKHR rayHitRegion = {};
        VkStridedDeviceAddressRegionKHR rayCallRegion = {};

    public:
        ~RayTracingPipelineImpl();
    };

    struct RayTracingPipeline : ImplHandle<RayTracingPipelineImpl>
    {
        RayTracingPipeline() = default;
        RayTracingPipeline(Context context);

    public:
        void Update(
            PipelineLayout layout,
            Span<Shader> rayGenShaders,
            Span<Shader> rayMissShaders,
            Span<HitShaderGroup> rayHitShaderGroup,
            Span<Shader> callableShaders) const;
    };

// -----------------------------------------------------------------------------

    struct RenderingDescription
    {
        Span<Format> colorFormats;
        Format        depthFormat = {};
        Format      stencilFormat = {};
    };

    struct CommandPoolImpl : ImplBase
    {
        ContextImpl* context = {};
        QueueImpl*     queue = {};

        VkCommandPool               pool = {};
        std::vector<CommandList> buffers = {};
        u32                        index = 0;

        std::vector<CommandList> secondaryBuffers = {};
        u32                        secondaryIndex = 0;

    public:
        ~CommandPoolImpl();
    };

    struct CommandPool : ImplHandle<CommandPoolImpl>
    {
        CommandPool() = default;
        CommandPool(Context context, Queue queue);

    public:
        CommandList Begin(ResourceTracker tracker) const;
        CommandList BeginSecondary(ResourceTracker tracker, OptRef<const RenderingDescription> renderingDescription = {}) const;

        void Reset() const;
    };

    struct CommandListImpl : ImplBase
    {
        CommandPoolImpl*        pool = {};
        ResourceTrackerImpl* tracker = {};
        VkCommandBuffer       buffer = {};
    };

    struct CommandList : ImplHandle<CommandListImpl>
    {
        CommandList() = default;

        void End() const;

        void UpdateBuffer(Buffer dst, const void* pData, usz size, u64 dstOffset = 0) const;
        void CopyToBuffer(Buffer dst, Buffer src, u64 size, u64 dstOffset = 0, u64 srcOffset = 0) const;
        void CopyToTexture(Texture dst, Buffer src, u64 srcOffset = 0) const;
        void GenerateMips(Texture texture) const;

        void Clear(Texture texture, Vec4 color) const;
        void Transition(Texture texture, VkImageLayout newLayout, VkPipelineStageFlags2 newStages, VkAccessFlags2 newAccess) const;
        void Transition(Texture texture, ResourceState state, BindPoint bindPoint) const;
        void Present(Texture texture) const;

        void SetViewport(Vec2U size, bool flipVertical) const;
        void SetTopology(Topology topology) const;
        void SetCullState(CullMode mode, FrontFace frontFace) const;
        void SetPolyState(PolygonMode poly, f32 lineWidth) const;
        void SetBlendState(u32 colorAttachmentCount, bool blendEnable) const;
        void SetDepthState(bool enable, bool write, CompareOp compareOp) const;

        void BeginRendering(Span<Texture> colorAttachments, Texture depthAttachment = {}, Texture stencilAttachment = {}, bool allowSecondary = false) const;
        void EndRendering() const;
        void ClearColor(u32 attachment, Vec4 color, Vec2U size, Vec2I offset = {}) const;
        void ClearDepth(f32 depth, Vec2U size, Vec2I offset = {}) const;
        void ClearStencil(u32 value, Vec2U size, Vec2I offset = {}) const;

        void BindDescriptorBuffers(Span<Buffer> buffers) const;
        void SetDescriptorSetOffsets(PipelineLayout layout, u32 firstSet, Span<DescriptorSetBindingOffset> offsets) const;

        void BindShaders(Span<Shader> shaders) const;
        void BindIndexBuffer(Buffer buffer, IndexType indexType, u64 offset = 0) const;
        void PushConstants(PipelineLayout layout, ShaderStage stages, u64 offset, u64 size, const void* data) const;

        void PushStorageTexture(PipelineLayout layout, u32 setIndex, u32 binding, Texture texture, u32 arrayIndex = 0) const;
        void PushAccelerationStructure(PipelineLayout layout, u32 setIndex, u32 binding, AccelerationStructure accelerationStructure, u32 arrayIndex = 0) const;

        void Draw(u32 vertices, u32 instances, u32 firstVertex, u32 firstInstance) const;
        void DrawIndexed(u32 indices, u32 instances, u32 firstIndex, u32 vertexOffset, u32 firstInstance) const;
        void ExecuteCommands(Span<CommandList> commands) const;

        void BuildAccelerationStructure(AccelerationStructureBuilder builder, AccelerationStructure structure, Buffer scratch) const;
        void CompactAccelerationStructure(AccelerationStructure dst, AccelerationStructure src) const;
        void BindPipeline(RayTracingPipeline pipeline) const;
        void TraceRays(RayTracingPipeline pipeline, Vec3U extent, u32 genIndex) const;
    };

// -----------------------------------------------------------------------------

    struct ContextImpl : ImplBase
    {
        VkInstance  instance = {};
        VkPhysicalDevice gpu = {};
        VkDevice      device = {};
        VmaAllocator     vma = {};

        VkDebugUtilsMessengerEXT debugMessenger = {};

        static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
            VkDebugUtilsMessageSeverityFlagBitsEXT severity,
            VkDebugUtilsMessageTypeFlagsEXT type,
            const VkDebugUtilsMessengerCallbackDataEXT* data,
            void* userData);

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

        Queue graphics = {};

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
        ~ContextImpl();
    };

    struct ContextConfig
    {
        bool debug = false;
        bool rayTracing = false;
    };

    struct Context : ImplHandle<ContextImpl>
    {
    public:
        Context() = default;
        Context(const ContextConfig& config);

        Context(ContextImpl* context)
            : ImplHandle(context)
        {}

    public:
        void WaitForIdle();

        Queue GetQueue(QueueFlags flags) const noexcept;
    };
}
