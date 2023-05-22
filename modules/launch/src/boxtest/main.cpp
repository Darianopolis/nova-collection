#include <nova/rhi/nova_RHI.hpp>
#include <nova/core/nova_Timer.hpp>

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <array>
#include <iostream>

using namespace nova::types;

void TryMain()
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    // glfwWindowHint(GLFW_MOUSE_PASSTHROUGH, GLFW_TRUE);
    auto window = glfwCreateWindow(1920, 1200, "next", nullptr, nullptr);
    NOVA_ON_SCOPE_EXIT(&) {
        glfwDestroyWindow(window);
        glfwTerminate();
    };

    auto context = nova::Context::Create(true);

    auto surface = context->CreateSurface(glfwGetWin32Window(window));
    auto swapchain = context->CreateSwapchain(surface,
        nova::ImageUsage::TransferDst
        | nova::ImageUsage::ColorAttach,
        nova::PresentMode::Fifo);

    auto queue = context->graphics;
    auto commandPool = context->CreateCommands();
    auto fence = context->CreateFence();
    auto tracker = context->CreateResourceTracker();

// -----------------------------------------------------------------------------

    struct PushConstants
    {
        alignas(8) Vec2 invHalfExtent;
        alignas(8) Vec2 centerPos;
        u64 instancesVA;
    };

    struct RoundedBox
    {
        Vec4 centerColor;
        Vec4 borderColor;
        Vec2 centerPos;
        Vec2 halfExtent;
        float cornerRadius;
        float borderWidth;
    };

    VkPushConstantRange range {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        .size = sizeof(PushConstants),
    };

    VkPipelineLayout layout;
    nova::VkCall(vkCreatePipelineLayout(context->device, nova::Temp(VkPipelineLayoutCreateInfo {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &range,
    }), context->pAlloc, &layout));

    auto instanceBuffer = context->CreateBuffer(sizeof(RoundedBox) * 3,
        nova::BufferUsage::Storage,
        nova::BufferFlags::DeviceLocal
        | nova::BufferFlags::CreateMapped);

    std::string preamble = R"(
#version 460

#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_buffer_reference2 : require

struct RoundedBox
{
    vec4 centerColor;
    vec4 borderColor;
    vec2 centerPos;
    vec2 halfExtent;
    float cornerRadius;
    float borderWidth;
};
layout(buffer_reference, scalar) buffer RoundedBoxRef { RoundedBox data[]; };

layout(push_constant) uniform PushConstants {
    vec2 invHalfExtent;
    vec2 centerPos;
    uint64_t instancesVA;
} pc;
    )";

    auto vs = context->CreateShader(
        nova::ShaderStage::Vertex, {},
        "vertex",
        preamble + R"(
const vec2[6] deltas = vec2[] (
    vec2(-1, -1),
    vec2(-1,  1),
    vec2( 1, -1),

    vec2(-1,  1),
    vec2( 1,  1),
    vec2( 1, -1)
);

layout(location = 0) out vec2 outTex;
layout(location = 1) out uint outInstanceID;

void main()
{
    RoundedBox box = RoundedBoxRef(pc.instancesVA).data[gl_InstanceIndex];
    vec2 delta = deltas[gl_VertexIndex % 6];
    outTex = delta * box.halfExtent;
    outInstanceID = gl_InstanceIndex;
    gl_Position = vec4(((delta * box.halfExtent) + box.centerPos - pc.centerPos) * pc.invHalfExtent, 0, 1);
}
        )",
        {range});

    auto fs = context->CreateShader(
        nova::ShaderStage::Fragment, {},
        "fragment",
        preamble + R"(
layout(location = 0) in vec2 inTex;
layout(location = 1) in flat uint inInstanceID;

layout(location = 0) out vec4 outColor;

void main()
{
    RoundedBox box = RoundedBoxRef(pc.instancesVA).data[inInstanceID];

    vec2 absPos = abs(inTex);
    vec2 cornerFocus = box.halfExtent - vec2(box.cornerRadius);

    if (absPos.x > cornerFocus.x && absPos.y > cornerFocus.y)
    {
        float dist = length(absPos - cornerFocus);
        if (dist > box.cornerRadius + 0.5)
            discard;

        if (dist > box.cornerRadius - box.borderWidth + 0.5)
            outColor = vec4(box.borderColor.rgb, box.borderColor.a * (1 - max(0, dist - (box.cornerRadius - 0.5))));
        else
            outColor = mix(box.centerColor, box.borderColor, max(0, dist - (box.cornerRadius - box.borderWidth - 0.5)));
    }
    else
    {
        if (absPos.x > box.halfExtent.x - box.borderWidth || absPos.y > box.halfExtent.y - box.borderWidth)
            outColor = vec4(vec3(0.2), 1.0);
        else
            outColor = box.centerColor;
    }
}
        )",
        {range});

// -----------------------------------------------------------------------------

    int count;
    auto mode = glfwGetVideoMode(glfwGetMonitors(&count)[0]);
    int mWidth = mode->width;
    int mHeight = mode->height;

    std::cout << "Monitor size = " << mWidth << ", " << mHeight << '\n';

    RoundedBox box1 {
        .centerColor = { 1.f, 0.f, 0.f, 0.5f },
        .borderColor = { 0.2f, 0.2f, 0.2f, 1.f },
        .centerPos = { mWidth * 0.25f, mHeight * 0.25f },
        .halfExtent = { 100.f, 100.f },
        .cornerRadius = 15.f,
        .borderWidth = 5.f,
    };

    RoundedBox box2 {
        .centerColor = { 0.f, 1.f, 0.f, 0.5f },
        .borderColor = { 0.2f, 0.2f, 0.2f, 1.f },
        .centerPos = { mWidth * 0.5f, mHeight * 0.5f },
        .halfExtent = { 100.f, 100.f },
        .cornerRadius = 15.f,
        .borderWidth = 10.f,
    };

    RoundedBox box3 {
        .centerColor = { 0.f, 0.f, 1.f, 0.5f },
        .borderColor = { 0.2f, 0.2f, 0.2f, 1.f },
        .centerPos = { mWidth * 0.75f, mHeight * 0.75f },
        .halfExtent = { 200.f, 100.f },
        .cornerRadius = 25.f,
        .borderWidth = 10.f,
    };

    instanceBuffer->Get<RoundedBox>(0) = box1;
    instanceBuffer->Get<RoundedBox>(1) = box2;
    instanceBuffer->Get<RoundedBox>(2) = box3;

    struct Rect {
        float left = INFINITY, right = -INFINITY, top = INFINITY, bottom = -INFINITY;
    };

    auto expandBounds = [](Rect& bounds, const RoundedBox& box)
    {
        bounds.left = std::min(bounds.left, box.centerPos.x - box.halfExtent.x);
        bounds.right = std::max(bounds.right, box.centerPos.x + box.halfExtent.x);
        bounds.top = std::min(bounds.top, box.centerPos.y - box.halfExtent.y);
        bounds.bottom = std::max(bounds.bottom, box.centerPos.y + box.halfExtent.y);
    };

    auto drawBox = [&](nova::CommandList* cmd, const Rect& bounds, u32 instanceID) {
        float width = bounds.right - bounds.left;
        float height = bounds.bottom - bounds.top;

        auto& box = instanceID == 0 ? box1 : instanceID == 1 ? box2 : box3;
        instanceBuffer->Get<RoundedBox>(instanceID) = box;

        cmd->PushConstants(layout,
            nova::ShaderStage::Vertex | nova::ShaderStage::Fragment,
            range.offset, range.size,
            nova::Temp(PushConstants {
                .invHalfExtent = Vec2(2.f / (bounds.right - bounds.left), 2.f / (bounds.bottom - bounds.top)),
                .centerPos = Vec2((bounds.left + bounds.right) / 2.f, (bounds.top + bounds.bottom) / 2.f),
                .instancesVA = instanceBuffer->address,
            }));

        cmd->Draw(6, 1, 0, instanceID);
    };

    bool redraw = true;
    bool skipUpdate = false;

    auto lastFrame = std::chrono::steady_clock::now();
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

// -----------------------------------------------------------------------------

        if (!skipUpdate)
        {
            auto moveBox = [&](RoundedBox& box, int left, int right, int up, int down) {
                float speed = 5.f;
                if (glfwGetKey(window, left))  { box.centerPos.x -= speed; redraw = true; }
                if (glfwGetKey(window, right)) { box.centerPos.x += speed; redraw = true; }
                if (glfwGetKey(window, up))    { box.centerPos.y -= speed; redraw = true; }
                if (glfwGetKey(window, down))  { box.centerPos.y += speed; redraw = true; }
            };

            moveBox(box1, GLFW_KEY_A, GLFW_KEY_D, GLFW_KEY_W, GLFW_KEY_S);
            moveBox(box2, GLFW_KEY_J, GLFW_KEY_L, GLFW_KEY_I, GLFW_KEY_K);
            moveBox(box3, GLFW_KEY_LEFT, GLFW_KEY_RIGHT, GLFW_KEY_UP, GLFW_KEY_DOWN);
        }
        else
        {
            redraw = true;
        }

        skipUpdate = false;

        if (!redraw)
        {
            glfwWaitEvents();
            skipUpdate = true;
        }
        redraw = false;

// -----------------------------------------------------------------------------

        Rect bounds;
        expandBounds(bounds, box1);
        expandBounds(bounds, box2);
        expandBounds(bounds, box3);

// -----------------------------------------------------------------------------

        // Record frame

        fence->Wait();
        commandPool->Reset();

        // auto cmd2 = commandPool->BeginSecondary(tracker, nova::RenderingDescription {
        //     .colorFormats = { nova::Format(swapchain->format.format) },
        // });
        // drawBox(cmd2, bounds, 0);
        // drawBox(cmd2, bounds, 1);
        // drawBox(cmd2, bounds, 2);
        // cmd2->End();

        auto cmd = commandPool->BeginPrimary(tracker);
        cmd->SetViewport({ bounds.right - bounds.left, bounds.bottom - bounds.top }, false);
        cmd->SetBlendState(1, true);
        cmd->SetTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
        cmd->BindShaders({vs, fs});

        // Update window size, record primary buffer and present

        glfwSetWindowSize(window, i32(bounds.right - bounds.left), i32(bounds.bottom - bounds.top));
        glfwSetWindowPos(window, i32(bounds.left), i32(bounds.top));

        queue->Acquire({swapchain}, {fence});

        cmd->BeginRendering({swapchain->image}, {Vec4(0.f)}, true);
        // cmd->ExecuteCommands({cmd2});

        drawBox(cmd, bounds, 0);
        drawBox(cmd, bounds, 1);
        drawBox(cmd, bounds, 2);
        cmd->EndRendering();

        cmd->Transition(swapchain->image, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

        queue->Submit({cmd}, {fence}, {fence});
        queue->Present({swapchain}, {fence});
    }
}

int main()
{
    try
    {
        TryMain();
    }
    catch(...) {}
}