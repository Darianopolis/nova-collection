#include "bldr.hpp"
#include "log.hpp"

#include <unordered_set>
#include <filesystem>
#include <array>
#include <fstream>
#include <algorithm>
#include <regex>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <any>

#include "Windows.h"
#include "shellapi.h"

std::string generate_env()
{
    auto env_file = s_paths.environments / "msvc";
    fs::create_directories(s_paths.environments);

    {
        std::ifstream in(env_file, std::ios::binary | std::ios::ate);
        if (in.is_open()) {
            std::string str;
            str.resize(in.tellg());
            in.seekg(0);
            in.read(str.data(), str.size());
            return str;
        }
    }

    // TODO: Move all of this process execution + input + output into unified helper
    STARTUPINFOA startup{};
    PROCESS_INFORMATION process{};

    std::string cmd = "cmd /c call \"C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvarsx86_amd64.bat\" && set";

    SECURITY_ATTRIBUTES sec_attribs;
    SecureZeroMemory(&sec_attribs, sizeof(sec_attribs));
    sec_attribs.nLength = sizeof(sec_attribs);
    sec_attribs.bInheritHandle = true;
    sec_attribs.lpSecurityDescriptor = nullptr;

    HANDLE stdout_read_handle;
    HANDLE stdout_write_handle;

    CreatePipe(&stdout_read_handle, &stdout_write_handle, &sec_attribs, 0);

    startup.cb = sizeof(startup);
    startup.hStdError = stdout_write_handle;
    startup.hStdOutput = stdout_write_handle;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    startup.dwFlags = STARTF_USESTDHANDLES;

    auto res = CreateProcessA(
        nullptr,
        cmd.data(),
        nullptr,
        nullptr,
        true,
        0,
        nullptr,
        nullptr,
        &startup,
        &process);

    if (!res) {
        log_error("Error creating environment: {}", GetLastError());
    } else {
        CloseHandle(stdout_write_handle);

        log_info("Generating MSVC environment...");

        std::string output;
        std::array<char, 4096> buffer{};
        DWORD bytes_read;
        BOOL success;
        for (;;) {
            success = ReadFile(stdout_read_handle, buffer.data(), DWORD(buffer.size()) - 1, &bytes_read, 0);
            if (!success || bytes_read == 0) break;
            output.append(buffer.data(), bytes_read);
        }

        WaitForSingleObject(process.hProcess, INFINITE);
        CloseHandle(stdout_read_handle);

        std::string env;
        std::string line;
        std::stringstream ss(output);
        while (std::getline(ss, line)) {
            if (line.ends_with('\r')) {
                line = line.substr(0, line.size() - 1);
            }
            if (line.find_first_of('=') != std::string::npos) {
                env.append(line);
                env.push_back('\0');
            }
        }
        env.push_back('\0');

        {
            std::ofstream out(env_file, std::ios::binary);
            out.write(env.data(), env.size());
        }

        log_info("MSVC environment generated");

        return env;
    }

    std::exit(1);
}

const std::string& get_build_environment()
{
    static std::string env = generate_env();
    return env;
}

uint32_t execute_program(const program_exec_t& info, flags_t flags)
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

    for (auto& arg : info.arguments) cmd(arg);

    std::string cmd_line = ss.str();
    if (is_set(flags, flags_t::trace)) {
        log_debug("{}", cmd_line);
    }

    STARTUPINFOA startup{};
    PROCESS_INFORMATION process{};

    auto res = CreateProcessA(
        nullptr,
        cmd_line.data(),
        nullptr,
        nullptr,
        false,
        NORMAL_PRIORITY_CLASS,
        (void*)get_build_environment().c_str(),
        info.working_directory.string().c_str(),
        &startup,
        &process);

    if (!res) {
        log_error("Error: {}", GetLastError());
    } else {
        WaitForSingleObject(process.hProcess, INFINITE);
        DWORD ec;
        GetExitCodeProcess(process.hProcess, &ec);
        return ec;
    }

    return 1;
}

void generate_build(project_artifactory_t& artifactory,  project_t& project, project_t& output)
{
    std::unordered_set<std::string_view> visited;

    auto insert_all = [](auto& target, auto& source)
    {
        target.insert_range(target.end(), source);
    };

    auto collect = [&](this auto&& self, project_t& cur_project)
    {
        if (visited.contains(cur_project.name)) return;
        visited.insert(cur_project.name);

        output.imports.push_back(cur_project.name);

        insert_all(output.includes,       cur_project.includes);
        insert_all(output.force_includes, cur_project.force_includes);
        insert_all(output.lib_paths,      cur_project.lib_paths);
        insert_all(output.links,          cur_project.links);
        insert_all(output.shared_libs,    cur_project.shared_libs);
        insert_all(output.build_defines,
            &cur_project == &project
                ? cur_project.build_defines
                : cur_project.defines);

        for (auto& import : cur_project.imports) {
            if (!artifactory.projects.contains(import)) {
                log_error("Could not find project with name '{}'", import);
                std::exit(1);
            }
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

        for (auto& file : resolve_glob(path)) insert_source(file);
    }

    output.name = project.name;
    output.artifact = project.artifact;
}

std::string to_string(const define_t& def)
{
    return def.value.empty() ? def.key : std::format("{}={}", def.key, def.value);
};

std::string to_string(const fs::path& path, char separator = '/')
{
    auto fspath = path.string();
    for (char& c : fspath) {
        if (c == '\\' || c == '/') {
            c = separator;
        }
    }
    return fspath;
}

// TODO: Prepass to build dependency tree
bool is_dirty(const project_t& project, const fs::path& path, fs::file_time_type last)
{
    std::unordered_set<fs::path> visited;

    // ^ \s* # \s* include \s+ ("|<) ([^">]+)
    std::regex regex(R"(^\s*#\s*include\s+("|<)([^">]+))");

    auto check = [&](this auto&& self, fs::path cur)
    {
        cur = fs::canonical(cur);

        if (visited.contains(cur)) {
            return false;
        }

        auto cur_last_write = fs::last_write_time(cur);
        if (cur_last_write > last) {
            return true;
        }

        visited.insert(cur);

        std::string str;
        std::ifstream in(cur, std::ios::binary | std::ios::ate);
        if (!in.is_open()) {
            log_warn("Couldn't open file {}", cur.string());
            return false;
        }
        str.resize(in.tellg());
        in.seekg(0);
        in.read(str.data(), str.size());

        auto beg = std::sregex_iterator(str.begin(), str.end(), regex);
        auto end = std::sregex_iterator();
        for (auto i = beg; i != end; ++i) {
            auto& match = *i;
            auto token = match.str(1);
            auto file = match.str(2);

            bool check_local = token == "\"";

            if (check_local) {
                auto new_path = cur.parent_path() / fs::path(file);
                if (fs::exists(new_path) && !fs::is_directory(new_path)) {
                    if (self(std::move(new_path))) {
                        return true;
                    }
                    continue;
                }
            }

            for (auto& include : project.includes) {
                auto new_path = include / file;
                if (fs::exists(new_path) && !fs::is_directory(new_path)) {
                    if (self(std::move(new_path))) {
                        return true;
                    }
                    break;
                }
            }
        }

        return false;
    };

    return check(path);
}

void build_project(std::span<project_t*> projects, flags_t flags)
{
    auto start = std::chrono::steady_clock::now();

    auto artifacts_dir = s_paths.artifacts;
    if (is_set(flags, flags_t::clean)) {
        log_info("Cleaning project artifacts");
        for (auto& project : projects) {
            fs::remove_all(artifacts_dir / project->name);
        }
    }

    for (auto& project : projects) {
        fs::create_directories(artifacts_dir / project->name);
    }

    struct compile_task_t
    {
        project_t* project;
        uint32_t source_idx;
    };

    std::vector<compile_task_t> compile_tasks;

    for (auto* project : projects) {
        for (uint32_t i = 0; i < project->sources.size(); ++i) {
            compile_tasks.emplace_back(project, i);
        }
    }

    std::atomic_uint32_t errors = 0;
    std::atomic_uint32_t aborted = 0;
    std::atomic_uint32_t skipped = 0;

    log_info("Compiling {} files...", compile_tasks.size());

#pragma omp parallel for
    for (int32_t i = 0; i < int32_t(compile_tasks.size()); ++i) {
        auto task = compile_tasks[i];
        auto& project = *task.project;
        auto& source = project.sources[task.source_idx];

        if (errors > 0) {
            aborted++;
            continue;
        }

        {
            auto target_obj = source.file;
            target_obj.replace_extension(".obj");
            target_obj = artifacts_dir / project.name / target_obj.filename();
            if (fs::exists(target_obj)) {
                auto last_modified = fs::last_write_time(target_obj);
                if (!is_dirty(project, source.file, last_modified)) {
                    skipped++;
                    continue;
                }
                fs::remove(target_obj);
            }
        }

        program_exec_t info;
        info.working_directory = {(artifacts_dir / project.name).string()};

        auto arg = [&](auto& exec_info, auto&&... args)
        {
            std::stringstream ss;
            (ss << ... << args);
            exec_info.arguments.push_back(ss.str());
        };

        arg(info, "cmd");
        arg(info, "/c");
        arg(info, "cl");

        // TODO: Parameterize

        arg(info, "/c");               // Compile without linking
        arg(info, "/nologo");          // Suppress banner
        arg(info, "/arch:AVX2");       // AVX2 vector extensions
        if (is_set(flags, flags_t::debug)) {
            arg(info, "/MDd");              // Use dynamic debug CRT
        } else {
            arg(info, "/MD");              // Use dynamic non-debug CRT
        }
        arg(info, "/Zc:preprocessor"); // Use conforming preprocessor
        arg(info, "/permissive-");     // Disable permissive mode
        // arg(info, "/fp:fast");         // Allow floating point reordering
        arg(info, "/utf-8");           // Set source and execution character sets
        arg(info, "/O2");              // Use maximum options
        arg(info, "/Ob3");             // Use maximum inlining

        arg(info, "/DUNICODE");  // Specify UNICODE for win32
        arg(info, "/D_UNICODE");

        arg(info, "/Z7");
        arg(info, "/DEBUG");

        if (is_set(flags, flags_t::debug)) {
            arg(info, "/DDEBUG");
            arg(info, "/D_DEBUG");
        }

        if (source.type == source_type_t::c) {
            arg(info, "/std:c17");
            arg(info, "/Tc", source.file.string());
        } else {
            arg(info, "/EHsc");           // Full exception unwinding
            arg(info, "/openmp:llvm");    // LLVM openmp (enables unsigned loop counters)
            arg(info, "/Zc:__cplusplus"); // Use correct __cplusplus macro value
            arg(info, "/std:c++latest");  // Use latest C++ language version
            if (source.type == source_type_t::cpp) {
                arg(info, "/experimental:module"); // Enable modules
                arg(info, "/translateInclude");    // Enable header include -> module import translation
            }
            arg(info, "/Tp", source.file.string());
        }

        for (auto& include : project.includes)       arg(info, "/I",  include.string());
        for (auto& include : project.force_includes) arg(info, "/FI", include.string());
        for (auto& define  : project.build_defines)  arg(info, "/D", to_string(define));

        auto res = execute_program(info, flags);
        if (res != 0) {
            errors++;
        }
    }

    if (errors == 0) {
        for (int32_t i = 0; i < projects.size(); ++i) {
            auto& project = *projects[i];
            if (!project.artifact) continue;

            auto& artifact = project.artifact.value();
            auto path = artifact.path;

            log_info("Generating [{}]", path.filename().string());

            program_exec_t info;
            info.working_directory = {artifacts_dir.string()};

            auto arg = [&](auto& exec_info, auto&&... args)
            {
                std::stringstream ss;
                (ss << ... << args);
                exec_info.arguments.push_back(ss.str());
            };

            arg(info, "cmd");
            arg(info, "/c");
            arg(info, "link");

            arg(info, "/nologo");
            arg(info, "/IGNORE:4099");    // PDB 'filename' was not found with 'object/library' or at 'path'; linking object as if no debug info
            arg(info, "/INCREMENTAL");    // TODO: Add option for full optimizing link
            arg(info, "/DYNAMICBASE:NO"); // Disable address space layout randomization.

            arg(info, "/DEBUG");

            if (is_set(flags, flags_t::debug)) {
                arg(info, "/NODEFAULTLIB:msvcrt.lib");
            } else {
                arg(info, "/NODEFAULTLIB:msvcrtd.lib");
            }
            arg(info, "/NODEFAULTLIB:libcmt.lib");
            arg(info, "/NODEFAULTLIB:libcmtd.lib");

            // Target

            if (artifact.type == artifact_type_t::console || artifact.type == artifact_type_t::window) {
                arg(info, "/SUBSYSTEM:", artifact.type == artifact_type_t::console ? "CONSOLE" : "WINDOWS");
                path.replace_extension(".exe");
            } else if (artifact.type == artifact_type_t::shared_library) {
                arg(info, "/DLL");
                path.replace_extension(".dll");
            }

            auto build_path = (artifacts_dir / project.name) / path.filename();
            arg(info, "/OUT:", build_path.string());

            // Library paths

            for (auto& lib_path : project.lib_paths) {
                arg(info, "/LIBPATH:", lib_path.string());
            }

            // Additional Links

            for (auto& link : project.links) {
                arg(info, link.string());
            }

            // Import objects

            for (auto& import : project.imports) {
                auto dir = artifacts_dir / import;
                if (!fs::exists(dir)) continue;
                auto iter = fs::directory_iterator(dir);
                for (auto& file : iter) {
                    if (file.path().extension() == ".obj") {
                        auto relative = fs::relative(file, artifacts_dir).string();
                        arg(info, relative);
                    }
                }
            }

            // Add default windows libraries

            arg(info, "user32.lib");
            arg(info, "gdi32.lib");
            arg(info, "shell32.lib");
            arg(info, "Winmm.lib");
            arg(info, "Advapi32.lib");
            arg(info, "Comdlg32.lib");
            arg(info, "comsuppw.lib");
            arg(info, "onecore.lib");

            arg(info, "D3D12.lib");
            arg(info, "DXGI.lib");
            arg(info, "dcomp.lib");
            arg(info, "d3d11.lib");

            // Link

            auto res = execute_program(info, flags);
            if (res != 0) {
                errors++;
                continue;
            }

            // Copy output

            try {
                fs::create_directories(path.parent_path());
                fs::remove(path);
                fs::copy(build_path, path);

                for (auto& shared_lib_glob : project.shared_libs) {
                    for (auto& shared_lib : resolve_glob(shared_lib_glob)) {
                        auto slib_target = path.parent_path() / shared_lib.filename();
                        fs::remove(slib_target);
                        fs::copy(shared_lib, slib_target);
                    }
                }
            } catch (const std::exception& e) {
                log_error("{}", e.what());
                errors++;
            }
        }
    }

    auto end = std::chrono::steady_clock::now();

    if (skipped > 0) {
        log_info("Skipped {} clean files", skipped.load());
    }

    if (errors == 0) {
        log_info("------------------------------------------------------------------------");
        log_info("\u001B[92mBuild Success\u001B[0m | Total time: {}", duration_to_string(end - start));
        log_info("------------------------------------------------------------------------");
    } else {
        if (aborted > 0) {
            log_warn("Aborted {} files after errors", aborted.load());
        }
        log_error("------------------------------------------------------------------------");
        log_error("\u001B[91mBuild Failure!\u001B[0m | Errors: {}", errors.load());
        log_error("------------------------------------------------------------------------");
    }
}

void configure_cmake(std::span<project_t*> projects, flags_t flags)
{
    auto artifacts_dir = s_paths.artifacts;

    auto local_bldr_dir = fs::path(".bldr");
    auto generated_marker = local_bldr_dir / "CMakeLists_generated";
    auto cmakelists_path = fs::path("CMakeLists.txt");

    if (fs::exists(cmakelists_path) && !fs::exists(generated_marker)) {
        log_error("CMakeLists found and not marked as generated");
        return;
    }

    {
        fs::create_directories(local_bldr_dir);
        std::ofstream out(generated_marker);
        out << std::chrono::utc_clock().now();
    }

    std::ofstream out(fs::path("CMakeLists.txt"), std::ios::binary);

    out << "cmake_minimum_required(VERSION 3.26)\n";
    out << "project(" << projects[0]->name << ")\n";
    out << "set(CMAKE_CXX_STANDARD 23)\n";
    out << "set(CMAKE_CXX_STANDARD_REQUIRED True)\n";
    out << "add_compile_options(\n";
    out << "        /Zc:preprocessor\n";
    out << "        /Zc:__cplusplus\n";
    out << "        /utf-8)\n";

    auto add_defines_and_includes = [&](project_t& project) {
        out << "target_compile_definitions(" << project.name << " PRIVATE";
        for (auto& define : project.build_defines) {
            out << "\n        -D" << to_string(define);
        }
        out << ")\n";

        out << "target_include_directories(" << project.name << " PRIVATE";
        for (auto& include : project.includes) {
            out << "\n        " << to_string(include);
        }
        out << ")\n";
    };

    for (int32_t i = 0; i < projects.size(); ++i) {
        auto& project = *projects[i];
        if (project.artifact) continue;

        if (!project.sources.empty()) {
            out << "add_library(" << project.name << " OBJECT";
            for (auto& source : project.sources) {
                out << "\n        " << to_string(source.file);
            }
            out << ")\n";
            add_defines_and_includes(project);
        }
    }

    for (int32_t i = 0; i < projects.size(); ++i) {
        auto& project = *projects[i];
        if (!project.artifact) continue;

        auto& artifact = project.artifact.value();
        auto path = artifact.path;

        auto name = project.name;

        out << "add_executable(" << name;
        for (auto& import : project.imports) {
            if (import == name) {
                for (auto& source : project.sources) {
                    out << "\n        " << to_string(source.file);
                }
            } else if (std::ranges::any_of(projects, [&](auto* p) { return p->name == import; })) {
                out << "\n        $<TARGET_OBJECTS:" << import << ">";
            } else {
                auto dir = artifacts_dir / import;
                if (!fs::exists(dir)) continue;
                auto iter = fs::directory_iterator(dir);
                for (auto& file : iter) {
                    if (file.path().extension() == ".obj") {
                        out << "\n        " << to_string(file.path());
                    }
                }
            }
        }
        out << ")\n";

        if (!project.sources.empty()) {
            add_defines_and_includes(project);
        }
        out << "target_link_libraries(" << name;
        // TODO: Libpaths and relative links
        for (auto& link_glob : project.links) {
            for (auto& link : resolve_glob(link_glob)) {
                out << "\n        " << to_string(link);
            }
        }
        out << ")\n";
        out << "set_target_properties(" << name << " PROPERTIES LINKER_LANGUAGE CXX)\n"; // TODO: C linking?
        if (!project.shared_libs.empty()) {
            out << "add_custom_command(TARGET " << name << " POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy -t $<TARGET_FILE_DIR:" << name << ">";
            for (auto& shared_lib_glob : project.shared_libs) {
                for (auto& shared_lib : resolve_glob(shared_lib_glob)) {
                    out << "\n        " << to_string(shared_lib);
                }
            }
            out << ")\n";
        }
        auto output_file = artifact.path;
        output_file.replace_extension(".exe");
        out << "add_custom_command(TARGET " << name << " POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:" << name << ">\n        " << to_string(output_file) << ")\n";
        if (!project.shared_libs.empty()) {
            out << "add_custom_command(TARGET " << name << " POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy -t " << to_string(artifact.path.parent_path());
            for (auto& shared_lib_glob : project.shared_libs) {
                for (auto& shared_lib : resolve_glob(shared_lib_glob)) {
                    out << "\n        " << to_string(shared_lib);
                }
            }
            out << ")\n";
        }
    }
}