#include "bldr.hpp"
#include "log.hpp"
#include "json.hpp"

#include <unordered_set>
#include <filesystem>
#include <array>
#include <fstream>
#include <algorithm>
#include <regex>
#include <ranges>

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
    static std::mutex process_start_mutex;

    std::unique_lock lock{ process_start_mutex };
    constexpr bool use_pseudo_console = false;

    STARTUPINFOEXA startup{};
    PROCESS_INFORMATION process{};

    SECURITY_ATTRIBUTES sec_attribs;
    SecureZeroMemory(&sec_attribs, sizeof(sec_attribs));
    sec_attribs.nLength = sizeof(sec_attribs);
    sec_attribs.bInheritHandle = true;
    sec_attribs.lpSecurityDescriptor = nullptr;

    HANDLE stdout_read_handle;
    HANDLE stdout_write_handle;

    HANDLE stdin_read_handle;
    HANDLE stdin_write_handle;

    CreatePipe(&stdout_read_handle, &stdout_write_handle, &sec_attribs, 0);
    CreatePipe(&stdin_read_handle, &stdin_write_handle, &sec_attribs, 0);

    startup.StartupInfo.cb = sizeof(startup);
    startup.StartupInfo.hStdError = stdout_write_handle;
    startup.StartupInfo.hStdOutput = stdout_write_handle;
    startup.StartupInfo.hStdInput = stdin_read_handle;
    startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;

    bool inherit_pipes = true;
    DWORD process_create_flags = NORMAL_PRIORITY_CLASS;

    if (use_pseudo_console) {
        inherit_pipes = false;
        process_create_flags |= EXTENDED_STARTUPINFO_PRESENT;

        COORD size = { 150, 1024 };

        HPCON pseudo_console;
        auto result = CreatePseudoConsole(size, stdin_read_handle, stdout_write_handle, 0, &pseudo_console);
        if (FAILED(result)) {
            return result;
        }

        // Discover the size required for the list
        size_t bytesRequired;
        InitializeProcThreadAttributeList(NULL, 1, 0, &bytesRequired);

        // Allocate memory to represent the list
        startup.lpAttributeList = (PPROC_THREAD_ATTRIBUTE_LIST)HeapAlloc(GetProcessHeap(), 0, bytesRequired);
        if (!startup.lpAttributeList) {
            return ~0u;
        }

        // Initialize the list memory location
        if (!InitializeProcThreadAttributeList(startup.lpAttributeList, 1, 0, &bytesRequired)) {
            HeapFree(GetProcessHeap(), 0, startup.lpAttributeList);
            return (uint32_t)GetLastError();
        }

        // Set the psueodconsole information into the list
        if (!UpdateProcThreadAttribute(startup.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, pseudo_console, sizeof(pseudo_console), NULL, NULL)) {
            HeapFree(GetProcessHeap(), 0, startup.lpAttributeList);
            return (uint32_t)GetLastError();
        }
    }

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

    using namespace std::chrono;
    auto start = steady_clock::now();

    auto res = CreateProcessA(
        nullptr,
        cmd_line.data(),
        nullptr,
        nullptr,
        inherit_pipes,
        process_create_flags,
        (void*)get_build_environment().c_str(),
        info.working_directory.string().c_str(),
        &startup.StartupInfo,
        &process);

    if (!res) {
        std::cout << std::format("Error running process: {:#x})\n", DWORD(HRESULT_FROM_WIN32(GetLastError())));
        return res;
    }

    CloseHandle(stdout_write_handle);
    CloseHandle(stdin_read_handle);

    uint64_t total_bytes_read = 0;

    {
        std::string line;
        std::jthread reader_thread {
            [&] {
                std::array<char, UINT16_MAX> buffer = {};
                DWORD bytes_read;
                BOOL success;
                for (;;) {
                    success = ReadFile(stdout_read_handle, buffer.data(), DWORD(buffer.size()), &bytes_read, 0);
                    if (!success || bytes_read == 0) break;
                    total_bytes_read += bytes_read;

                    // TODO: Time lock holding to avoid interspersing output from multiple processes
                    for (char c : std::string_view(buffer.data(), bytes_read)) {
                        line.push_back(c);
                        if (c == '\n') {
                            lock.lock();
                            std::cout << line;
                            lock.unlock();
                            line.clear();
                        }
                    }
                }
            }
        };

        lock.unlock();
        WaitForSingleObject(process.hProcess, INFINITE);
        lock.lock();

        // Flush remainder of output buffer
        std::cout << line;

        DWORD ec;
        GetExitCodeProcess(process.hProcess, &ec);

        CloseHandle(stdout_read_handle);
        CloseHandle(stdin_write_handle);

        CloseHandle(process.hProcess);
        CloseHandle(process.hThread);

        return uint32_t(ec);
    }
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
            // log_warn("Couldn't open file {}", cur.string());
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

bool build_project(std::span<project_t*> projects, flags_t flags)
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

    // Find list of changed files
    // TODO: Try linearizing and heavily memoizing this

    std::vector<uint32_t> filtered_compile_tasks;

#pragma omp parallel for
    for (int32_t i = 0; i < int32_t(compile_tasks.size()); ++i) {
        auto task = compile_tasks[i];
        auto& project = *task.project;
        auto& source = project.sources[task.source_idx];

        auto target_obj = source.file;
        target_obj.replace_extension(".obj");
        target_obj = artifacts_dir / project.name / target_obj.filename();
        if (fs::exists(target_obj)) {
            auto last_modified = fs::last_write_time(target_obj);
            if (!is_dirty(project, source.file, last_modified)) {
                continue;
            }
            fs::remove(target_obj);
        }

#pragma omp critical
        {
            filtered_compile_tasks.emplace_back(i);
        }
    }

    log_info("Compiling {} file{} ({} skipped)",
        filtered_compile_tasks.size(),
        (filtered_compile_tasks.size() == 1) ? "" : "s",
        compile_tasks.size() - filtered_compile_tasks.size());

    std::atomic_uint32_t errors = 0;
    std::atomic_uint32_t aborted = 0;

#pragma omp parallel for
    for (int32_t i = 0; i < int32_t(filtered_compile_tasks.size()); ++i) {
        auto task = compile_tasks[filtered_compile_tasks[i]];
        auto& project = *task.project;
        auto& source = project.sources[task.source_idx];

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
            arg(info, "/MDd");         // Use dynamic debug CRT
        } else {
            arg(info, "/MD");          // Use dynamic non-debug CRT
        }
        arg(info, "/Zc:preprocessor"); // Use conforming preprocessor
        arg(info, "/permissive-");     // Disable permissive mode
        // arg(info, "/fp:fast");      // Allow floating point reordering
        arg(info, "/utf-8");           // Set source and execution character sets
        arg(info, "/O2");              // Maximum optimization level
        arg(info, "/Ob3");             // Maximum inlining level

        arg(info, "/cgthreads8");      // threads for optimization + code generation

        arg(info, "/DUNICODE");        // Specify UNICODE for win32
        arg(info, "/D_UNICODE");

        if (is_set(flags, flags_t::lto)) {
            arg(info, "/GL");          // Enable whole program optimization
            arg(info, "/Gw");          // Optimize global data
        }

        if (!is_set(flags, flags_t::strip)) {
            arg(info, "/Z7");          // Generate debug info and include in object files
            arg(info, "/DEBUG");
        }

        arg(info, "/constexpr:steps10000000"); // Increase constexpr step limit

        arg(info, "/D_CRT_SECURE_NO_WARNINGS"); // Suppress MS "security" warnings

        if (!is_set(flags, flags_t::nowarn)) {
            arg(info, "/W4");     // Warning level 4
            arg(info, "/WX");     // Warnings as errors

            arg(info, "/we4289"); // nonstandard extension used: 'variable': loop control variable declared in the for-loop is used outside the for-loop scope

            arg(info, "/w14242"); // 'identifier': conversion from 'type1' to 'type1', possible loss of data
            arg(info, "/w14254"); // 'operator': conversion from 'type1:field_bits' to 'type2:field_bits', possible loss of data
            arg(info, "/w14263"); // 'function': member function does not override any base class virtual member function
            arg(info, "/w14265"); // 'classname': class has virtual functions, but destructor is not virtual instances of this class may not be destructed correctly
            arg(info, "/w14287"); // 'operator': unsigned/negative constant mismatch
            arg(info, "/w14296"); // 'operator': expression is always 'boolean_value'
            arg(info, "/w14311"); // 'variable': pointer truncation from 'type1' to 'type2'
            arg(info, "/w14545"); // expression before comma evaluates to a function which is missing an argument list
            arg(info, "/w14546"); // function call before comma missing argument list
            arg(info, "/w14547"); // 'operator': operator before comma has no effect; expected operator with side effect
            arg(info, "/w14549"); // 'operator': operator before comma has no effect; did you intend 'operator'?
            arg(info, "/w14555"); // expression has no effect; expected expression with side- effect
            arg(info, "/w14640"); // Enable warning on thread unsafe static member initialization
            arg(info, "/w14826"); // Conversion from 'type1' to 'type_2' is sign-extended. This may cause unexpected runtime behavior.
            arg(info, "/w14905"); // wide string literal cast to 'LPSTR'
            arg(info, "/w14906"); // string literal cast to 'LPWSTR'
            arg(info, "/w14928"); // illegal copy-initialization; more than one user-defined conversion has been implicitly applied

            arg(info, "/wd4324"); // 'struct': structure was padded due to alignment specifier
            arg(info, "/wd4505"); // 'function': unreferenced function with internal linkage has been removed

            arg(info, "/permissive-"); // standards conformance mode for MSVC compiler.
        }

        if (is_set(flags, flags_t::debug)) {
            arg(info, "/DDEBUG");   // Enable debug checks
            arg(info, "/D_DEBUG");
        }

        if (source.type == source_type_t::c) {
            arg(info, "/std:c23");
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
            arg(info, "/IGNORE:4099");     // PDB 'filename' was not found with 'object/library' or at 'path'; linking object as if no debug info
            if (is_set(flags, flags_t::lto)) {
                arg(info, "/LTCG");
            } else {
                arg(info, "/INCREMENTAL"); // Incremental build when not optimizing
            }
            arg(info, "/DYNAMICBASE:NO");  // Disable address space layout randomization.

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

    if (errors == 0) {
        log_info("------------------------------------------------------------------------");
        log_info("\u001B[92mBuild Success\u001B[0m | Total time: {}", duration_to_string(end - start));
        log_info("------------------------------------------------------------------------");
        return true;
    } else {
        if (aborted > 0) {
            log_warn("Aborted {} files after errors", aborted.load());
        }
        log_error("------------------------------------------------------------------------");
        log_error("\u001B[91mBuild Failure!\u001B[0m | Errors: {}", errors.load());
        log_error("------------------------------------------------------------------------");
        return false;
    }
}

void configure_vscode(std::span<project_t*> projects, flags_t)
{
    log_info("Configuring VSCode C/C++ build for MS and clangd extensions");
    auto c_cpp_properties_path = fs::path(".vscode/c_cpp_properties.json");

    fs::create_directories(c_cpp_properties_path.parent_path());

    std::ofstream out(c_cpp_properties_path, std::ios::binary);
    json_writer_t json(out);

    // TODO: Create small JSON writer

    json.object();
    json["configurations"].array();
    json.object();

    json["name"] = "MSVC";
    json["compilerArgs"].array();
    {
        json << "/Zc:preprocessor";
        json << "/Zc:__cplusplus";
        json << "/utf-8";
        // TODO: Modules
    }
    json.end_array();

    // Defines

    auto defined = std::unordered_set<std::string>();
    defined.insert("WIN32");
    defined.insert("UNICODE");
    defined.insert("_UNICODE");
    for (int32_t i = 0; i < projects.size(); ++i) {
        auto& project = *projects[i];

        for (auto& define : project.build_defines) {
            if (!define.value.empty()) {
                defined.insert(std::format("{}={}", define.key, define.value));
            } else {
                defined.insert(define.key);
            }
        }
    }
    json["defines"].array();
    for (auto& define : defined)
        json << define;
    json.end_array();

    // Force includes

    auto force_included = std::unordered_set<std::string>();
    for (int32_t i = 0; i < projects.size(); ++i) {
        auto& project = *projects[i];
        for (auto& fi : project.force_includes) {
            force_included.insert(to_string(fi));
        }
    }
    json["forcedInclude"].array();
    for (auto& include : force_included) {
        json << include;
    }
    json.end_array();

    // Includes

    auto included = std::unordered_set<std::string>();
    for (int32_t i = 0; i < projects.size(); ++i) {
        auto& project = *projects[i];
        for (auto& include : project.includes) {
            included.insert(to_string(include));
        }
    }
    json["includePath"].array();
    for (auto& include : included) {
        json << include;
    }
    json.end_array();

    // Compiler settings

    json["cStandard"]         = "c23";
    json["cppStandard"]       = "c++23";
    json["intelliSenseMode"]  = "windows-msvc-x64";
    json["windowsSdkVersion"] = "10.0.22621.0";
    json["compilerPath"] = "cl.exe";

    // End

    json.end_object();
    json.end_array();
    json.end_object();

    log_info("Configured successfully!");
}

void configure_cmake(std::span<project_t*> projects, flags_t)
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
            out << "\n        \"" << to_string(include) << "\"";
        }
        out << ")\n";
    };

    for (int32_t i = 0; i < projects.size(); ++i) {
        auto& project = *projects[i];
        if (project.artifact) continue;

        if (!project.sources.empty()) {
            out << "add_library(" << project.name << " OBJECT";
            for (auto& source : project.sources) {
                out << "\n        \"" << to_string(source.file) << "\"";
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
                    out << "\n        \"" << to_string(source.file) << "\"";
                }
            } else if (std::ranges::any_of(projects, [&](auto* p) { return p->name == import; })) {
                out << "\n        $<TARGET_OBJECTS:" << import << ">";
            } else {
                auto dir = artifacts_dir / import;
                if (!fs::exists(dir)) continue;
                auto iter = fs::directory_iterator(dir);
                for (auto& file : iter) {
                    if (file.path().extension() == ".obj") {
                        out << "\n        \"" << to_string(file.path()) << "\"";
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
                out << "\n        \"" << to_string(link) << "\"";
            }
        }
        out << ")\n";
        out << "set_target_properties(" << name << " PROPERTIES LINKER_LANGUAGE CXX)\n"; // TODO: C linking?
        if (!project.shared_libs.empty()) {
            out << "add_custom_command(TARGET " << name << " POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy -t $<TARGET_FILE_DIR:" << name << ">";
            for (auto& shared_lib_glob : project.shared_libs) {
                for (auto& shared_lib : resolve_glob(shared_lib_glob)) {
                    out << "\n        \"" << to_string(shared_lib) << "\"";
                }
            }
            out << ")\n";
        }
        auto output_file = artifact.path;
        output_file.replace_extension(".exe");
        out << "add_custom_command(TARGET " << name << " POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:" << name << ">\n        \"" << to_string(output_file) << "\")\n";
        if (!project.shared_libs.empty()) {
            out << "add_custom_command(TARGET " << name << " POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy -t " << to_string(artifact.path.parent_path());
            for (auto& shared_lib_glob : project.shared_libs) {
                for (auto& shared_lib : resolve_glob(shared_lib_glob)) {
                    out << "\n        \"" << to_string(shared_lib) << "\"";
                }
            }
            out << ")\n";
        }
    }
}