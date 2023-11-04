#include "bldr.hpp"

#include <unordered_set>
#include <filesystem>
#include <array>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include "Windows.h"
#include "shellapi.h"

namespace fs = std::filesystem;

void generate_build(project_artifactory_t& artifactory,  project_t& project, project_t& output)
{
    std::unordered_set<std::string_view> visited;

    auto insert_all = [](auto& target, auto& source)
    {
        target.insert_range(target.end(), source);
    };

    auto collect = [&](this auto&& self, project_t& project)
    {
        if (visited.contains(project.name)) return;
        visited.insert(project.name);

        output.imports.push_back(project.name);

        insert_all(output.includes,       project.includes);
        insert_all(output.force_includes, project.force_includes);
        insert_all(output.lib_paths,      project.lib_paths);
        insert_all(output.links,          project.links);
        insert_all(output.build_defines,
            &project == &project
                ? project.build_defines
                : project.defines);

        for (auto& import : project.imports) {
            self(*artifactory.projects.at(import));
        }
    };

    collect(project);

    for (auto&[path, type] : project.sources) {
        auto insert_source = [&](const fs::path& file)
        {
            auto src_type = type;
            if (type == source_type_t::automatic) {
                if      (file.extension() == ".cppm") src_type = source_type_t::cppm;
                else if (file.extension() == ".ixx" ) src_type = source_type_t::cppm;
                else if (file.extension() == ".cpp" ) src_type = source_type_t::cpp;
                else if (file.extension() == ".cxx" ) src_type = source_type_t::cpp;
                else if (file.extension() == ".cc"  ) src_type = source_type_t::cpp;
                else if (file.extension() == ".c"   ) src_type = source_type_t::c;
            }

            if (src_type != source_type_t::automatic) {
                output.sources.push_back({ {file.string()}, src_type });
            }
        };

        if (path.path.ends_with("/**")) {
            auto iter = fs::recursive_directory_iterator(path.to_fspath(3));
            for (auto& file : iter) insert_source(file.path());
        } else if (path.path.ends_with("/*")) {
            auto iter = fs::directory_iterator(path.to_fspath(2));
            for (auto& file : iter) insert_source(file.path());
        } else {
            insert_source(path.to_fspath());
        }
    }

    output.name = project.name;
    output.artifact = project.artifact;
}

void build_project(project_t& project)
{
    auto artifacts_dir = std::filesystem::current_path() / "artifacts";

    {
        program_exec_t info;
        auto dir = artifacts_dir / project.name;
        std::filesystem::create_directories(dir);
        info.working_directory = {dir.string()};
        info.executable = {std::string("cl.exe")};

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

#pragma omp parallel for
        for (uint32_t i = 0; i < project.sources.size(); ++i) {
            auto& source = project.sources[i];
            execute_program(info, std::array<std::string_view, 2> {
                "/Tp",
                source.file.to_fspath().string(),
            });
        }
    }

    if (project.artifact) {
        program_exec_t info;
        info.working_directory = {artifacts_dir.string()};
        info.executable = {std::string("link.exe")};

        auto arg = [&](auto& exec_info, auto&&... args)
        {
            std::stringstream ss;
            (ss << ... << args);
            exec_info.arguments.push_back(ss.str());
        };

        arg(info, "/nologo");
        arg(info, "/IGNORE:4099");
        arg(info, "/DYNAMICBASE:NO");
        arg(info, "/NODEFAULTLIB:msvcrtd.lib");
        arg(info, "/NODEFAULTLIB:libcmt.lib");
        arg(info, "/NODEFAULTLIB:libcmtd.lib");
        arg(info, "/SUBSYSTEM:CONSOLE");
        arg(info, "/OUT:", project.artifact->path.to_fspath().string());

        for (auto& lib_path : project.lib_paths) {
            arg(info, "/LIBPATH:", lib_path.to_fspath().string());
        }

        for (auto& link : project.links) {
            arg(info, link.to_fspath().string());
        }

        for (auto& import : project.imports) {
            auto dir = artifacts_dir / import;
            if (!std::filesystem::exists(dir)) continue;
            auto iter = std::filesystem::directory_iterator(dir);
            for (auto& file : iter) {
                if (file.path().extension() == ".obj") {
                    auto relative = std::filesystem::relative(file, artifacts_dir).string();
                    arg(info, relative);
                }
            }
        }

        arg(info, "user32.lib");
        arg(info, "gdi32.lib");
        arg(info, "shell32.lib");
        arg(info, "Winmm.lib");
        arg(info, "Advapi32.lib");
        arg(info, "Comdlg32.lib");
        arg(info, "comsuppw.lib");
        arg(info, "onecore.lib");

        execute_program(info, {});
    }
}

void execute_program(const program_exec_t& info, std::span<const std::string_view> additional_arguments)
{
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
    // std::cout << "Running command:\n" << cmd_line << '\n';

    STARTUPINFOA startup{};
    PROCESS_INFORMATION process{};

    auto res = CreateProcessA(
        nullptr,
        cmd_line.data(),
        nullptr,
        nullptr,
        false,
        NORMAL_PRIORITY_CLASS,
        nullptr,
        info.working_directory.to_fspath().string().c_str(),
        &startup,
        &process);

    if (!res) {
        std::cout << "Error: " << GetLastError() << '\n';
    } else {
        WaitForSingleObject(process.hProcess, INFINITE);
    }
}