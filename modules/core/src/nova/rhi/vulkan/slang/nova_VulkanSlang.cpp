#include <nova/rhi/vulkan/nova_VulkanRHI.hpp>

#include <nova/core/nova_Core.hpp>
#include <nova/core/nova_Files.hpp>
#include <nova/vfs/nova_VirtualFilesystem.hpp>
#include <nova/rhi/slang/nova_SlangCompiler.hpp>

namespace nova
{
    std::vector<uint32_t> Vulkan_CompileSlangToSpirv(
            ShaderStage       /* stage */,
            StringView           entry,
            StringView        filename,
            Span<StringView> fragments)
    {
        NOVA_ASSERT(fragments.empty(), "slang must be compiled through virtual filesystem resources");

        static SlangCompiler compiler;

        nova::Log("────────────────────────────────────────────────────────────────────────────────");
        nova::Log("Compiling slang to spirv");
        nova::Log("  entry = {}", entry);
        nova::Log("  filename = {}", filename);

        auto session = compiler.CreateSession();
        auto slang_module = session.Load(filename);

        nova::Log("module loaded");
        nova::Log("  available entry points");
        for (auto& e : slang_module.GetEntryPointNames()) {
            nova::Log("  - {}", e);
        }
        nova::Log("  module dependencies");
        for (auto& e : session.GetLoadedPaths()) {
            nova::Log("  - {}", e);
        }

        return slang_module.GenerateCode(entry);
    }

    static
    std::monostate slang_loaded = Vulkan_RegisterCompiler(ShaderLang::Slang, Vulkan_CompileSlangToSpirv);
}