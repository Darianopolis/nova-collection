#include <nova/rhi/nova_RHI.hpp>
#include <nova/core/nova_Timer.hpp>

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <array>
#include <iostream>

using namespace nova::types;

int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    // glfwWindowHint(GLFW_MOUSE_PASSTHROUGH, GLFW_TRUE);
    auto window = glfwCreateWindow(1920, 1200, "next", nullptr, nullptr);

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
        alignas(8) glm::vec2 pos;
        alignas(8) glm::vec2 size;
        alignas(16) glm::vec4 color;

        // alignas(8) Vec2 invHalfExtent;
        // alignas(8) Vec2 centerPos;
        // u64 instancesVA;
    };

    // struct RoundedBox
    // {
    //     Vec4 centerColor;
    //     Vec4 borderColor;
    //     Vec2 centerPos;
    //     Vec2 halfExtent;
    //     float cornerRadius;
    //     float borderWidth;
    // };

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

    std::string preamble = R"(
#version 460

// #extension GL_EXT_scalar_block_layout : require
// #extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
// #extension GL_EXT_buffer_reference2 : require

// struct RoundedBox
// {
//     vec4 centerColor;
//     vec4 borderColor;
//     vec2 centerPos;
//     vec2 halfExtent;
//     float cornerRadius;
//     float borderWidth;
// }
// layout(buffer_reference, scalar) RoundedBoxRef { RoundedBox data[]; };

layout(push_constant) uniform PushConstants {
    vec2 pos;
    vec2 size;
    vec4 color;

    // vec2 invHalfExtent;
    // vec2 centerPos;
    // uint64_t instancesVA;
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

// layout(location = 0) out uint outInstanceID;

layout(location = 0) out vec2 outTex;

void main()
{
    // RoundedBox box = RoundedBoxRef(pc.instancesVA).data[gl_InstanceIndex];
    // vec2 delta = deltas[gl_VertexIndex % 6];
    // outTex = delta * box.halfExtent;
    // gl_Position = vec4((delta - pc.centerPos) * pc.invHalfExtent, 0, 1);

    outTex = deltas[gl_VertexIndex] * 100;
    gl_Position = vec4(deltas[gl_VertexIndex] * pc.size + pc.pos, 0, 1);
}
        )",
        {range});

    auto fs = context->CreateShader(
        nova::ShaderStage::Fragment, {},
        "fragment",
        preamble + R"(
// layout(location = 0) in uint inInstanceID;
layout(location = 0)  in vec2 inTex;

layout(location = 0) out vec4 outColor;

void main()
{
    // RoundedBox box = RoundedBoxRef(pc.instancesVA).data[inInstanceID];

    // vec4 border = box.borderColor;
    // vec4 center = box.centerColor;
    // vec2 halfExtent = box.halfExtent;
    // float cornerRadius = box.cornerRadius;
    // float borderWidth = box.borderWidth;

    vec4 border = vec4(vec3(0.2), 1);
    vec4 center = pc.color;
    vec2 halfExtent = vec2(100, 100);
    float cornerRadius = 15.0;
    float borderWidth = 5.0;

    vec2 cornerFocus = halfExtent - vec2(cornerRadius);

    if (abs(inTex.x) > cornerFocus.x && abs(inTex.y) > cornerFocus.y)
    {
        vec2 pos = vec2(abs(inTex.x), abs(inTex.y));
        float dist = length(pos - cornerFocus);
        if (dist > cornerRadius + 0.5)
            discard;

        if (dist > cornerRadius - borderWidth + 0.5)
            outColor = vec4(border.rgb, border.a * (1 - max(0, dist - (cornerRadius - 0.5))));
        else
            outColor = mix(center, border, max(0, dist - (cornerRadius - borderWidth - 0.5)));
    }
    else
    {
        if (abs(inTex.x) > halfExtent.x - borderWidth || abs(inTex.y) > halfExtent.y - borderWidth)
            outColor = vec4(vec3(0.2), 1.0);
        else
            outColor = center;
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

    struct Box {
        float x, y, w, h;
        Vec4 color;
    };

    Box box1 {
        .x = mWidth * 0.25f,
        .y = mHeight * 0.25f,
        .w = 100.f,
        .h = 100.f,
        .color = Vec4(1.f, 0.f, 0.f, 0.5f),
    };

    Box box2 {
        .x = mWidth * 0.5f,
        .y = mHeight * 0.5f,
        .w = 100.f,
        .h = 100.f,
        .color = Vec4(0.f, 1.f, 0.f, 0.5f),
    };

    Box box3 {
        .x = mWidth * 0.75f,
        .y = mHeight * 0.75f,
        .w = 100.f,
        .h = 100.f,
        .color = Vec4(0.f, 0.f, 1.f, 0.5f),
    };

    struct Rect {
        float left = INFINITY, right = -INFINITY, top = INFINITY, bottom = -INFINITY;
    };

    auto expandBounds = [](Rect& bounds, const Box& box)
    {
        bounds.left = std::min(bounds.left, box.x - box.w);
        bounds.right = std::max(bounds.right, box.x + box.w);
        bounds.top = std::min(bounds.top, box.y - box.h);
        bounds.bottom = std::max(bounds.bottom, box.y + box.h);
    };

    auto drawBox = [&](nova::CommandList* cmd, const Rect& bounds, const Box& box) {
        float width = bounds.right - bounds.left;
        float height = bounds.bottom - bounds.top;

        Box out;
        out.x = (2.f * (box.x - bounds.left) / width) - 1.f;
        out.y = (2.f * (box.y - bounds.top) / height) - 1.f;
        out.w = 2.f * box.w / width;
        out.h = 2.f * box.w / height;

        cmd->PushConstants(layout,
            nova::ShaderStage::Vertex | nova::ShaderStage::Fragment,
            range.offset, range.size,
            nova::Temp(PushConstants {
                .pos = { out.x, out.y },
                .size = { out.w, out.h },
                .color = box.color,
            }));

        cmd->Draw(6, 1, 0, 0);
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
            auto moveBox = [&](Box& box, int left, int right, int up, int down) {
                float speed = 5.f;
                if (glfwGetKey(window, left))  { box.x -= speed; redraw = true; }
                if (glfwGetKey(window, right)) { box.x += speed; redraw = true; }
                if (glfwGetKey(window, up))    { box.y -= speed; redraw = true; }
                if (glfwGetKey(window, down))  { box.y += speed; redraw = true; }
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

        auto cmd2 = commandPool->BeginSecondary(tracker, nova::RenderingDescription {
            .colorFormats = { nova::Format(swapchain->format.format) },
        });
        drawBox(cmd2, bounds, box1);
        drawBox(cmd2, bounds, box2);
        drawBox(cmd2, bounds, box3);
        cmd2->End();

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
        cmd->ExecuteCommands({cmd2});
        cmd->EndRendering();

        cmd->Transition(swapchain->image, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

        queue->Submit({cmd}, {fence}, {fence});
        queue->Present({swapchain}, {fence});
    }

    glfwDestroyWindow(window);
    glfwTerminate();
}