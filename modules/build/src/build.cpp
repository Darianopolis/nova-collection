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
#include <spanstream>

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

uint32_t execute_program(const program_exec_t& info, [[maybe_unused]] flags_t flags, std::string_view name)
{
    (void)name;
    // auto start = std::chrono::steady_clock::now();

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

    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = true;
    sa.lpSecurityDescriptor = nullptr;

    HANDLE stdout_read = {};
    HANDLE stdout_write = {};

    // Create a pipe for the child process' STDOUT.
    if (!CreatePipe(&stdout_read, &stdout_write, &sa, 0)) {
        log_error("Failed to create pipe for executing process");
        return 1;
    }

    // Ensure the read handle to the pipe for STDOUT is not inherit
    if (!SetHandleInformation(stdout_read, HANDLE_FLAG_INHERIT, 0)) {
        log_error("Failed to disable inheritance for stdout read handle");
        CloseHandle(stdout_write);
        CloseHandle(stdout_read);
        return 1;
    }

    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    si.hStdError = stdout_write;
    si.hStdOutput = stdout_write;
    si.dwFlags |= STARTF_USESTDHANDLES;

    PROCESS_INFORMATION pi = {};

    if (!CreateProcessA(
            nullptr,
            cmd_line.data(),
            nullptr,
            nullptr,
            true, // Inherit Handles
            0,    // Creation flags
            (void*)get_build_environment().c_str(),
            info.working_directory.string().c_str(),
            &si,
            &pi)) {
        log_error("Failed to launch process");
        CloseHandle(stdout_write);
        CloseHandle(stdout_read);
        return 1;
    }

    std::jthread reader_thread {
        [&] {
            std::string line;
            for (;;) {
                std::array<char, UINT16_MAX> buffer = {};
                DWORD bytes_read;
                if (!ReadFile(stdout_read, buffer.data(), DWORD(buffer.size()), &bytes_read, 0)) {
                    break;
                }

                // TODO: Used timed locks holding to avoid interspersing output from multiple processes
                for (char c : std::string_view(buffer.data(), bytes_read)) {
                    line.push_back(c);
                    if (c == '\n') {
                        std::cout << line;
                        line.clear();
                    }
                }
            }

            // Flush remainder of output buffer
            if (!line.empty()) {
                std::cout << line << '\n';
            }
        }
    };

    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD ec;
    GetExitCodeProcess(pi.hProcess, &ec);

    CloseHandle(stdout_write);
    CloseHandle(stdout_read);
    CloseHandle(pi.hProcess);

    // auto end = std::chrono::steady_clock::now();
    // log_debug("{} - {}", name, duration_to_string(end - start));

    return ec;
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
            if (!fs::is_regular_file(file)) return;

            auto src_type = type;
            if (type == source_type_t::automatic) {
                if      (file.extension() == ".cppm" ) src_type = source_type_t::cppm;
                else if (file.extension() == ".ixx"  ) src_type = source_type_t::cppm;
                else if (file.extension() == ".cpp"  ) src_type = source_type_t::cpp;
                else if (file.extension() == ".cxx"  ) src_type = source_type_t::cpp;
                else if (file.extension() == ".cc"   ) src_type = source_type_t::cpp;
                else if (file.extension() == ".c"    ) src_type = source_type_t::c;
                else if (file.extension() == ".slang") src_type = source_type_t::slang;
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

// -----------------------------------------------------------------------------

struct file_dep_t
{
    std::string name;
    bool       local;
};

struct file_includes_t
{
    bool                       dirty = false;
    bool                     visited = false;
    bool                     scanned = false;
    std::string                 name;
    std::vector<file_dep_t> includes;
    // TODO: Store file scan time and use to invalidate scan state
};

std::unordered_map<std::string_view, std::unique_ptr<file_includes_t>> file_cache;

struct timer_t
{
    std::string_view                       name;
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    bool                                running = true;

    void end()
    {
        // if (!running) return;

        // running = false;

        // auto end = std::chrono::steady_clock::now();
        // log_debug("Task[{}] took {}", name, duration_to_string(end - start));
    }

    void segment(std::string_view _name)
    {
        // end();
        // start = std::chrono::steady_clock::now();
        // running = true;
        name = _name;
    }

    ~timer_t()
    {
        end();
    }
};

static constexpr std::string_view s_file_index_name = ".include-cache";

void load_file_include_cache()
{
    timer_t t{ "load file index" };

    auto path = s_paths.dir / s_file_index_name;

    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        return;
    }
    std::vector<char> data;
    data.resize(fs::file_size(path));
    in.read(data.data(), data.size());

    std::spanstream din{data};

    auto ReadUInt32 = [&] {
        uint32_t v;
        din.read((char*)&v, 4);
        return v;
    };

    auto ReadString = [&] {
        auto size = ReadUInt32();
        std::string str(size, '\0');
        din.read(str.data(), str.size());
        return str;
    };

    auto count = ReadUInt32();

    for (uint32_t i = 0; i < count; ++i) {
        auto file = std::make_unique<file_includes_t>();
        file->name = ReadString();
        file->includes.resize(ReadUInt32());
        file->scanned = true;
        // log_debug("loaded file [{}]", file->name);
        for (uint32_t j = 0; j < file->includes.size(); ++j) {
            file->includes[j].local = bool(ReadUInt32());
            file->includes[j].name = ReadString();
        }
        file_cache[file->name] = std::move(file);
    }
}

void save_file_include_cache()
{
    // TODO: Only save index if changed

    timer_t t{ "save file index" };

    auto path = s_paths.dir / s_file_index_name;

    std::ofstream out(path, std::ios::binary);

    auto WriteUInt32 = [&](uint32_t v) {
        out.write((const char*)&v, 4);
    };

    auto WriteString = [&](std::string_view s) {
        WriteUInt32(uint32_t(s.size()));
        out.write(s.data(), s.size());
    };

    uint32_t count = 0;
    for (auto& file : std::views::values(file_cache)) {
        count += file->scanned;
    }

    WriteUInt32(count);

    for (auto& file : std::views::values(file_cache)) {
        if (!file->scanned) continue;
        WriteString(file->name);
        WriteUInt32(uint32_t(file->includes.size()));
        for (auto& dep : file->includes) {
            WriteUInt32(dep.local);
            WriteString(dep.name);
        }
    }
}

static size_t files_visited = 0;
static size_t files_scanned = 0;
static size_t bytes_scanned = 0;

// ^ \s* # \s* include \s+ ("|<) ([^">]+)
static const std::regex regex(R"(^\s*#\s*include\s+("|<)([^">]+))");

bool is_dirty(const project_t& project, const fs::path& path, fs::file_time_type last)
{
    auto check = [&](this auto&& self, fs::path cur)
    {
        cur = fs::canonical(cur);

        files_visited++;

        file_includes_t* state = {};
        {
            auto i = file_cache.find(cur.string());
            if (i != file_cache.end()) {
                state = i->second.get();
            } else {
                auto file = std::make_unique<file_includes_t>();
                state = file.get();
                file->name = cur.string();
                file_cache[file->name] = std::move(file);
            }
        }

        if (state->visited) {
            return state->dirty;
        }

        state->visited = true;

        if (!fs::exists(cur) || fs::last_write_time(cur) > last) {
            state->dirty = true;
            state->scanned = false;
            return true;
        }

        if (!state->scanned) {

            std::string str;
            std::ifstream in(cur, std::ios::binary | std::ios::ate);
            if (!in.is_open()) {
                // log_warn("Couldn't open file {}", cur.string());
                return false;
            }
            str.resize(in.tellg());
            in.seekg(0);
            in.read(str.data(), str.size());

            files_scanned++;
            bytes_scanned += str.size();
            state->scanned = true;

            auto beg = std::sregex_iterator(str.begin(), str.end(), regex);
            auto end = std::sregex_iterator();
            for (auto i = beg; i != end; ++i) {
                auto& match = *i;
                auto token = match.str(1);
                auto file = match.str(2);

                state->includes.push_back(file_dep_t {
                    .name = file,
                    .local = token == "\"",
                });
            }
        }

        for (auto& dep : state->includes) {
            if (dep.local) {
                auto new_path = cur.parent_path() / fs::path(dep.name);
                if (fs::exists(new_path) && !fs::is_directory(new_path)) {
                    if (self(std::move(new_path))) {
                        state->dirty = true;
                        return true;
                    }
                    continue;
                }
            }

            for (auto& include : project.includes) {
                auto new_path = include / dep.name;
                if (fs::exists(new_path) && !fs::is_directory(new_path)) {
                    if (self(std::move(new_path))) {
                        state->dirty = true;
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

// -----------------------------------------------------------------------------


bool build_project(std::span<project_t*> projects, flags_t flags)
{
    auto start = std::chrono::steady_clock::now();

    timer_t timer{ "clean & collect tasks" };

    auto artifacts_dir = s_paths.artifacts;
    if (is_set(flags, flags_t::clean)) {
        log_info("Cleaning project artifacts");
        for (auto& project : projects) {
            fs::remove_all(artifacts_dir / project->name);
        }
    }

    struct compile_task_t
    {
        project_t* project;
        source_t    source;
    };

    std::vector<compile_task_t> compile_tasks;

    for (auto* project : projects) {
        for (uint32_t i = 0; i < project->sources.size(); ++i) {
            compile_tasks.emplace_back(project, project->sources[i]);
        }
    }

    // Find list of changed files

    std::vector<compile_task_t>       filtered_compile_tasks;
    std::unordered_set<std::filesystem::path> generated_objs;
    std::unordered_set<std::filesystem::path>       obj_dirs;

    for (auto& project : projects) {
        auto obj_dir = artifacts_dir / project->name;
        fs::create_directories(obj_dir);
        obj_dirs.emplace(obj_dir);
    }

    timer.segment("scan for changes");

    for (int32_t i = 0; i < int32_t(compile_tasks.size()); ++i) {
        auto task = compile_tasks[i];
        auto& project = *task.project;
        auto& source = task.source;

        auto target_obj = source.file;
        if (source.type == source_type_t::embed) {
            target_obj = std::format("{}.obj", target_obj.string());
        } else if (source.type == source_type_t::slang) {
            target_obj.replace_extension(".spv.obj");
        } else {
            target_obj.replace_extension(".obj");
        }
        target_obj = artifacts_dir / project.name / target_obj.filename();

        generated_objs.emplace(target_obj);

        if (fs::exists(target_obj)) {
            auto last_modified = fs::last_write_time(target_obj);
            if (source.type == source_type_t::embed) {
                // Don't look for dependencies in embeds
                if (fs::last_write_time(source.file) <= last_modified) {
                    continue;
                }
            } else {
                if (!is_dirty(project, source.file, last_modified)) {
                    continue;
                }
            }
            fs::remove(target_obj);
        }

        filtered_compile_tasks.emplace_back(compile_tasks[i]);
    }

    log_info("Scanned {} / {} ({} bytes)", files_scanned, files_visited, bytes_scanned);

    log_info("Compiling {} file{} ({} skipped)",
        filtered_compile_tasks.size(),
        (filtered_compile_tasks.size() == 1) ? "" : "s",
        compile_tasks.size() - filtered_compile_tasks.size());

    timer.segment("compiling");

    std::atomic_uint32_t errors = 0;
    std::atomic_uint32_t aborted = 0;

    auto arg = [&](auto& exec_info, auto&&... args)
    {
        std::stringstream ss;
        (ss << ... << args);
        exec_info.arguments.push_back(ss.str());
    };

    auto args = [&](auto& exec_info, auto&&... _args)
    {
        (arg(exec_info, _args), ...);
    };

    while (filtered_compile_tasks.size()) {
        std::vector<compile_task_t> new_compile_tasks;

#pragma omp parallel for
        for (int32_t i = 0; i < int32_t(filtered_compile_tasks.size()); ++i) {
            auto task = filtered_compile_tasks[i];
            auto& project = *task.project;
            auto source = task.source;

            program_exec_t info;
            info.working_directory = {(artifacts_dir / project.name).string()};

            bool generated = false;

            if (source.type == source_type_t::embed) {
                log("embedding {}...", source.file.filename().string());

                auto gen_path = artifacts_dir / project.name / std::format("{}.cpp", source.file.filename().string());
                std::ofstream gen(gen_path, std::ios::binary);
                gen << "#include <cstdint>\n";
                gen << "void bldr_register_embed(const char* name, const void* data, size_t size_in_bytes);\n";
                gen << "namespace {\n";
                gen << "int bldr_register_embed_call(const char* name, const void* data, size_t size_in_bytes)\n";
                gen << "{\n";
                gen << "    bldr_register_embed(name, data, size_in_bytes);\n";
                gen << "    return 1;\n";
                gen << "}\n";
                gen << "constexpr uint64_t bldr_embed_data[] {";

                auto resource_in_path = source.file;
                std::ifstream resource_in(resource_in_path, std::ios::binary);
                size_t resource_byte_size = fs::file_size(resource_in_path);
                size_t u64_count = (resource_byte_size + 7) / 8;

                for (size_t j = 0; j < u64_count; ++j) {
                    if ((j % 8) == 0) gen << '\n';
                    uint64_t value = {};
                    resource_in.read(reinterpret_cast<char*>(&value), sizeof(value));
                    gen << std::format("{:#016x},", value);
                }

                gen << "\n";
                gen << "};\n";
                gen << "const int bldr_embed_registered = bldr_register_embed_call(\"" << resource_in_path.filename().string() << "\", bldr_embed_data, " << resource_byte_size << ");\n";
                gen << "}\n";

                // Continue to compile generated source file

                resource_in.close();
                gen.close();

                source.type = source_type_t::cpp;
                source.file = gen_path;
                info.arguments.clear();
                generated = true;

            } else if (source.type == source_type_t::slang) {
                args(info, "cmd", "/c", "slangc");

                args(info, "-o", fs::path(source.file.filename()).replace_extension(".spv").string());

                args(info, "-lang", "slang");

                arg(info, "-matrix-layout-column-major");
                arg(info, "-force-glsl-scalar-layout");

                args(info, "-target", "spirv");
                arg(info, "-fvk-use-entrypoint-name");
                arg(info, "-emit-spirv-directly");

                for (auto& include : project.includes) arg(info, "-I",  include.string());

                log("{}", source.file.filename().string());
                arg(info, source.file.string());

                auto res = execute_program(info, flags, source.file.filename().string());
                if (res != 0) {
                    errors++;
                    continue;
                }

                // Generate source file for embedding

                auto gen_path = artifacts_dir / project.name / fs::path(source.file.filename()).replace_extension(".spv.cpp");
                std::ofstream gen(gen_path, std::ios::binary);
                gen << "#include <cstdint>\n";
                gen << "void bldr_register_embed(const char* name, const void* data, size_t size_in_bytes);\n";
                gen << "namespace {\n";
                gen << "int bldr_register_embed_call(const char* name, const void* data, size_t size_in_bytes)\n";
                gen << "{\n";
                gen << "    bldr_register_embed(name, data, size_in_bytes);\n";
                gen << "    return 1;\n";
                gen << "}\n";
                gen << "constexpr uint32_t bldr_spirv_data[] {";

                auto spirv_in_path = artifacts_dir / project.name / fs::path(source.file.filename()).replace_extension(".spv");
                std::ifstream spirv_in(spirv_in_path, std::ios::binary);
                size_t spirv_byte_size = fs::file_size(spirv_in_path);
                size_t spirv_count = spirv_byte_size / 4;

                for (size_t j = 0; j < spirv_count; ++j) {
                    if ((j % 16) == 0) gen << '\n';
                    uint32_t value;
                    spirv_in.read(reinterpret_cast<char*>(&value), sizeof(value));
                    gen << std::format("{},", value);
                }

                gen << "\n";
                gen << "};\n";
                gen << "const int bldr_spirv_registered = bldr_register_embed_call(\"" << spirv_in_path.filename().string() << "\", bldr_spirv_data, " << spirv_byte_size << ");\n";
                gen << "}\n";

                // Continue to compile generated source file

                spirv_in.close();
                gen.close();

                source.type = source_type_t::cpp;
                source.file = gen_path;
                info.arguments.clear();
                generated = true;
            }

            if (generated) {
#pragma omp critical
                {
                    new_compile_tasks.emplace_back(&project, source);
                }
                continue;
            }


            if (is_set(flags, flags_t::trace)) {
                log_debug("compiling [{}] into [{}]", source.file.filename().string(), to_string(info.working_directory));
            }

            args(info, "cmd", "/c", "cl");

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
            arg(info, "/Zc:char8_t-");     // Treat u8"" strings as char instead of char8_t

            if (!is_set(flags, flags_t::noopt)) {
                arg(info, "/O2");          // Maximum optimization level
                arg(info, "/Ob3");         // Maximum inlining level
            }

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
            auto res = execute_program(info, flags, source.file.filename().string());
            if (res != 0) {
                log_error("Process failed with code: {}", res);
                errors++;
            }
        }

        filtered_compile_tasks = std::move(new_compile_tasks);
    }

    timer.segment("cleaning old objects");

    bool any_objs_deleted = false;

    for (auto& obj_dir : obj_dirs) {
        if (is_set(flags, flags_t::trace)) {
            log_debug("Cleaning out obj dir [{}]", to_string(obj_dir));
        }
        for (auto& i : fs::directory_iterator(obj_dir)) {
            if (i.path().extension() != ".obj") continue;
            if (!generated_objs.contains(i.path())) {
                if (is_set(flags, flags_t::trace)) {
                    log_debug("Removing stale object [{}]", to_string(i.path().filename()));
                }
                fs::remove(i.path());
                any_objs_deleted = true;
            }
        }
    }

    timer.segment("linking");

    if (errors == 0) {
        for (int32_t i = 0; i < projects.size(); ++i) {
            auto& project = *projects[i];
            if (!project.artifact) continue;

            auto& artifact = project.artifact.value();
            auto path = artifact.path;

            program_exec_t info;
            info.working_directory = {artifacts_dir.string()};

            args(info, "cmd", "/c", "link", "/nologo");

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

            bool any_changed = is_set(flags, flags_t::clean) || is_set(flags, flags_t::link);
            auto output_last_write = fs::exists(build_path) ? fs::last_write_time(build_path) : std::filesystem::file_time_type();

            // TODO: Search for changes through additional links via libpaths

            // TODO: Relink when objs removed
            //       This only catches objects that were *just* removed, and misses any removed indirectly from other builds
            any_changed |= any_objs_deleted;

            for (auto& import : project.imports) {
                auto dir = artifacts_dir / import;
                if (!fs::exists(dir)) continue;
                auto iter = fs::directory_iterator(dir);
                for (auto& file : iter) {
                    if (file.path().extension() == ".obj") {
                        auto relative = fs::relative(file, artifacts_dir).string();
                        arg(info, relative);
                        if (fs::last_write_time(file.path()) > output_last_write) {
                            any_changed |= true;
                        }
                    }
                }
            }

            // // Mark dlls as lazy load
            // // TODO: ONLY do this for dlls with a specific flag set in the bldr script
            // {
            //     for (auto& shared_lib_glob : project.shared_libs) {
            //         for (auto& shared_lib : resolve_glob(shared_lib_glob)) {
            //             arg(info, "/DELAYLOAD:", shared_lib.filename().string());
            //             // auto slib_target = path.parent_path() / shared_lib.filename();
            //             // fs::remove(slib_target);
            //             // fs::copy(shared_lib, slib_target);
            //         }
            //     }
            // }
            // arg(info, "delayimp.lib");


            // Check if any link inputs changed

            if (any_changed) {
                log_info("Generating [{}]", path.filename().string());

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

                // Write link commands to file

                {
                    std::ofstream link_out(s_paths.dir / ".link-commands");
                    for (uint32_t j = 4; j < info.arguments.size(); ++j) {
                        link_out << '"' << info.arguments[j] << "\"\n";
                    }
                    info.arguments.resize(4);
                    arg(info, "@", to_string(s_paths.dir / ".link-commands"));

                }

                // Link

                auto res = execute_program(info, flags, path.filename().string());
                if (res != 0) {
                    errors++;
                    continue;
                }
            }

            // Copy output

            try {
                fs::create_directories(path.parent_path());

                if (any_changed || !fs::exists(path) || fs::last_write_time(build_path) > fs::last_write_time(path)) {
                    fs::remove(path);
                    fs::copy(build_path, path);
                }

                for (auto& shared_lib_glob : project.shared_libs) {
                    for (auto& shared_lib : resolve_glob(shared_lib_glob)) {
                        auto slib_target = path.parent_path() / shared_lib.filename();
                        if (!fs::exists(slib_target) || fs::last_write_time(shared_lib) > fs::last_write_time(slib_target)) {
                            fs::remove(slib_target);
                            fs::copy(shared_lib, slib_target);
                        }
                    }
                }
            } catch (const std::exception& e) {
                log_error("{}", e.what());
                errors++;
            }
        }
    }

    timer.segment("report");

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
    static constexpr std::string_view cmake_generated_tag_comment = "# Generated by bldr";

    auto artifacts_dir = s_paths.artifacts;

    auto cmakelists_path = fs::path("CMakeLists.txt");

    {
        std::ifstream in(cmakelists_path, std::ios::binary);
        if (in.is_open()) {
            std::string line;
            std::getline(in, line);
            if (line != cmake_generated_tag_comment) {
                log_error("CMakeLists found not marked as generated");
                return;
            }
        }
    }

    std::ofstream out(cmakelists_path, std::ios::binary);

    out << cmake_generated_tag_comment << '\n';
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