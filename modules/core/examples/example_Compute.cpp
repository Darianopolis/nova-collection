#include "main/example_Main.hpp"

#include <nova/rhi/nova_RHI.hpp>

#include <nova/core/nova_Guards.hpp>
#include <nova/core/nova_Timer.hpp>

#include <nova/window/nova_Window.hpp>

#include <nova/asset/nova_EditImage.hpp>

NOVA_EXAMPLE(Compute, "compute")
{
    if (args.size() < 2) {
        NOVA_LOG("Usage: <encoding> <file>");
        return;
    }

    auto encoding = args[0];
    {
        bool found_encoding = false;
        for (auto& _enc : { "rgba", "bc1", "bc2", "bc3", "bc4", "bc5", "bc6", "bc7" }) {
            if (encoding == _enc) {
                found_encoding = true;
                break;
            }
        }
        if (!found_encoding) {
            NOVA_LOG("Unrecognized encoding: {}", encoding);
            NOVA_LOG("Encodings: rgba, bc1, bc2, bc3, bc4, bc5, bc6, bc7");
            return;
        }
    }

    auto file = args[1];
    if (!std::filesystem::exists(file)) {
        NOVA_LOG("Could not file: {}", file);
        return;
    }

// -----------------------------------------------------------------------------
//                             GLFW Initialization
// -----------------------------------------------------------------------------

    auto app = nova::Application::Create();
    NOVA_DEFER(&) { app.Destroy(); };
    auto window = nova::Window::Create(app, {
        .title = "Nova - Compute",
        .size = { 800, 800 },
    });

// -----------------------------------------------------------------------------
//                             Nova Initialization
// -----------------------------------------------------------------------------

    auto context = nova::Context::Create({
        .debug = true,
        .compatibility = true,
    });
    NOVA_DEFER(&) { context.Destroy(); };

    // Create surface and swapchain for GLFW window

    auto swapchain = nova::Swapchain::Create(context, window.GetNativeHandle(),
        nova::ImageUsage::Storage
        | nova::ImageUsage::TransferDst
        | nova::ImageUsage::ColorAttach,
        nova::PresentMode::Immediate);
    NOVA_DEFER(&) { swapchain.Destroy(); };

    // Create required Nova objects

    auto queue = context.GetQueue(nova::QueueFlags::Graphics, 0);
    auto fence = nova::Fence::Create(context);
    auto wait_values = std::array { 0ull, 0ull };
    auto command_pools = std::array {
        nova::CommandPool::Create(context, queue),
        nova::CommandPool::Create(context, queue)
    };
    NOVA_DEFER(&) {
        command_pools[0].Destroy();
        command_pools[1].Destroy();
        fence.Destroy();
    };

    // Image

    auto sampler = nova::Sampler::Create(context, nova::Filter::Linear, nova::AddressMode::Edge, {}, 16.f);
    NOVA_DEFER(&) { sampler.Destroy(); };

    nova::Image image;
    NOVA_DEFER(&) { image.Destroy(); };
    {
        NOVA_TIMEIT_RESET();
        auto loaded = nova::EditImage::LoadFromFile(file);
        if (!loaded) NOVA_THROW("Failed to load image");

        NOVA_TIMEIT("image-load");
        NOVA_LOG("Loaded, size = {}, {}", loaded->extent.x, loaded->extent.y);
        NOVA_TIMEIT_RESET();

        window.SetSize(loaded->extent, nova::WindowPart::Client);

        std::vector<b8> data;
        nova::Format format;
        if (encoding == "rgba") {
            data = loaded->ConvertToPacked({ 0, 1, 2, 3 }, 1, false);
            format = nova::Format::RGBA8_UNorm;
        } else if (encoding == "bc1") {
            data = loaded->ConvertToBC1(true);
            format = nova::Format::BC1A_UNorm;
        } else if (encoding == "bc2") {
            data = loaded->ConvertToBC2();
            format = nova::Format::BC2_UNorm;
        } else if (encoding == "bc3") {
            data = loaded->ConvertToBC3();
            format = nova::Format::BC3_UNorm;
        } else if (encoding == "bc4") {
            data = loaded->ConvertToBC4(false);
            format = nova::Format::BC4_UNorm;
        } else if (encoding == "bc5") {
            data = loaded->ConvertToBC5(false);
            format = nova::Format::BC5_UNorm;
        } else if (encoding == "bc6") {
            data = loaded->ConvertToBC6(false);
            format = nova::Format::BC6_UFloat;
        } else if (encoding == "bc7") {
            data = loaded->ConvertToBC7();
            format = nova::Format::BC7_Unorm;
        }
        NOVA_TIMEIT("image-encode");
        image = nova::Image::Create(context,
            Vec3U(loaded->extent, 0u),
            nova::ImageUsage::Sampled | nova::ImageUsage::TransferDst,
            format);
        image.Set({}, image.GetExtent(), data.data());
        image.Transition(nova::ImageLayout::Sampled);
        NOVA_TIMEIT("image-upload");
    }

    // Shaders

    struct PushConstants
    {
        u32   image;
        u32 sampler;
        u32  target;
        Vec2   size;
    };

    auto hlsl_shader = nova::Shader::Create(context,
            nova::ShaderLang::Hlsl, nova::ShaderStage::Compute, "main", "", {R"hlsl(
            [[vk::binding(0, 0)]] Texture2D               Image2D[];
            [[vk::binding(1, 0)]] RWTexture2D<float4> RWImage2DF4[];
            [[vk::binding(2, 0)]] SamplerState            Sampler[];

            struct PushConstants {
                uint          image;
                uint linear_sampler;
                uint         target;
                float2         size;
            };

            [[vk::push_constant]] ConstantBuffer<PushConstants> pc;

            [numthreads(16, 16, 1)]
            void main(uint2 id: SV_DispatchThreadID) {
                float2 uv = float2(id) / pc.size;
                float4 source = Image2D[pc.image].SampleLevel(Sampler[pc.linear_sampler], uv, 0);
                if (source.a < 0.5) source = float4(1, 0, 1, 1);
                RWImage2DF4[pc.target][id] = source;
            }
        )hlsl"});
    NOVA_DEFER(&) { hlsl_shader.Destroy(); };

    auto glsl_shader = nova::Shader::Create(context,
            nova::ShaderLang::Glsl, nova::ShaderStage::Compute, "main", "", {R"glsl(
            #extension GL_EXT_scalar_block_layout  : require
            #extension GL_EXT_nonuniform_qualifier : require
            #extension GL_EXT_shader_explicit_arithmetic_types_int16 : require
            #extension GL_EXT_shader_image_load_formatted  : require

            layout(set = 0, binding = 0) uniform texture2D Image2D[];
            layout(set = 0, binding = 1) uniform image2D RWImage2D[];
            layout(set = 0, binding = 2) uniform sampler   Sampler[];

            layout(push_constant, scalar) uniform PushConstants {
                uint          image;
                uint linear_sampler;
                uint         target;
                vec2           size;
            } pc;

            layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;
            void main() {
                ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
                vec2 uv = vec2(pos) / pc.size;
                vec4 source = texture(sampler2D(Image2D[pc.image], Sampler[pc.linear_sampler]), uv);
                if (source.a < 0.5) source = vec4(1, 0, 1, 1);
                imageStore(RWImage2D[pc.target], pos, source);
            }
        )glsl"});
    NOVA_DEFER(&) { glsl_shader.Destroy(); };

    // Alternate shaders each frame

    nova::Shader shaders[] = { glsl_shader, hlsl_shader };

// -----------------------------------------------------------------------------
//                               Main Loop
// -----------------------------------------------------------------------------

    auto last_time = std::chrono::steady_clock::now();
    u64 frame_index = 0;
    u64 frames = 0;
    NOVA_DEFER(&) { fence.Wait(); };
    while (app.IsRunning()) {

        // Debug output statistics
        frames++;
        auto new_time = std::chrono::steady_clock::now();
        if (new_time - last_time > 1s) {
            NOVA_LOG("Frametime = {:.3f} ({} fps)", 1e6 / frames, frames);
            last_time = std::chrono::steady_clock::now();
            frames = 0;
        }

        u32 fif = frame_index++ % 2;

        // Wait for previous frame and acquire new swapchain image

        fence.Wait(wait_values[fif]);
        queue.Acquire({swapchain}, {fence});
        auto target = swapchain.GetCurrent();

        // Start new command buffer

        command_pools[fif].Reset();
        auto cmd = command_pools[fif].Begin();

        // Transition ready for writing compute output

        cmd.Transition(target,
            nova::ImageLayout::GeneralImage,
            nova::PipelineStage::Compute);

        // Dispatch

        cmd.PushConstants(PushConstants {
            .image = image.GetDescriptor(),
            .sampler = sampler.GetDescriptor(),
            .target = swapchain.GetCurrent().GetDescriptor(),
            .size = Vec2(swapchain.GetExtent()),
        });
        cmd.BindShaders({shaders[frame_index % 2]});
        cmd.Dispatch(Vec3U((Vec2U(target.GetExtent()) + 15u) / 16u, 1));

        // Submit and present work

        cmd.Present(swapchain);
        queue.Submit({cmd}, {fence}, {fence});
        queue.Present({swapchain}, {fence});

        // Wait for window events

        app.PollEvents();

        wait_values[fif] = fence.GetPendingValue();
    }
}