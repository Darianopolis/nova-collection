#include "bldr.hpp"

#include <array>

constexpr std::string_view s_compiler = "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.37.32822/bin/Hostx64/x64/cl.exe"sv;
constexpr std::string_view s_linker = "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.37.32822/bin/Hostx64/x64/link.exe"sv;

void build_project(project_t& project)
{
    {
        program_exec_t info;
        info.working_directory = {"C:/Users/Darian/.bldr-cpp/artifacts/bldr"};
        info.executable = {std::string(s_compiler)};

        auto arg = [&](auto& exec_info, auto&&... args)
        {
            std::stringstream ss;
            (ss << ... << args);
            exec_info.arguments.push_back(ss.str());
        };

        arg(info, "/c");
        arg(info, "/nologo");
        arg(info, "/arch:AVX512");
        arg(info, "/EHsc");
        arg(info, "/MD");
        arg(info, "/openmp:llvm");
        arg(info, "/Zc:preprocessor");
        arg(info, "/Zc:__cplusplus");
        arg(info, "/fp:fast");
        arg(info, "/utf-8");
        arg(info, "/permissive-");
        arg(info, "/O2");
        arg(info, "/INCREMENTAL");
        arg(info, "/EHsc");
        arg(info, "/Zc:__cplusplus");
        arg(info, "/std:c++latest");
        arg(info, "/experimental:module");
        arg(info, "/translateInclude");

        for (auto& include : project.includes)       arg(info, "/I",  include.to_fspath().string());
        for (auto& include : project.force_includes) arg(info, "/FI", include.to_fspath().string());
        for (auto& define  : project.build_defines) {
            if (define.value.empty()) arg(info, "/D", define.key);
            else                      arg(info, "/D", define.key, "=", define.value);
        }

        for (auto& source : project.sources) {
            execute_program(info, std::array<std::string_view, 2> {
                "/Tp",
                source.file.to_fspath().string(),
            });
        }
    }

    {
        program_exec_t info;
        info.working_directory = {"C:/Users/Darian/.bldr-cpp/artifacts"};
        info.executable = {std::string(s_linker)};

        auto arg = [&](auto& exec_info, auto&&... args)
        {
            std::stringstream ss;
            (ss << ... << args);
            exec_info.arguments.push_back(ss.str());
        };

        arg(info, "/IGNORE:4099");
        arg(info, "/DYNAMICBASE:NO");
        arg(info, "/NODEFAULTLIB:msvcrtd.lib");
        arg(info, "/NODEFAULTLIB:libcmt.lib");
        arg(info, "/NODEFAULTLIB:libcmtd.lib");
        arg(info, "/SUBSYSTEM:CONSOLE");
        arg(info, "/OUT:", project.artifact.path.to_fspath().string());

        for (auto& lib_path : project.lib_paths) {
            arg(info, "/LIBPATH:", lib_path.to_fspath().string());
        }

        for (auto& import : project.imports) {
            auto dir = info.working_directory.to_fspath() / import;
            if (!std::filesystem::exists(dir)) continue;
            auto iter = std::filesystem::directory_iterator(dir);
            for (auto& file : iter) {
                if (file.path().extension() == ".obj") {
                    arg(info, file.path().string());
                }
            }
        }

        execute_program(info, {});
    }
}

void execute_program(const program_exec_t& info, std::span<const std::string_view> additional_arguments)
{
    std::filesystem::current_path(info.working_directory.to_fspath());

    std::stringstream ss;
    auto cmd = [&](std::string_view value)
    {
        if (value.contains(' ')) {
            ss << '"' << value << "\" ";
        } else {
            ss << value << ' ';
        }
    };

    cmd(info.executable.to_fspath().string());
    for (auto& arg : info.arguments) cmd(arg);
    for (auto& arg : additional_arguments) cmd(arg);

    std::string cmd_line = ss.str();
    std::cout << "Running command:\n" << cmd_line << '\n';

    std::system(cmd_line.c_str());
}