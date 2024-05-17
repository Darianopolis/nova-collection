#include <excpt.h>

#include "bldr.hpp"
#include <log.hpp>

#include <sol/sol.hpp>

#include <unordered_set>
#include <fstream>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

struct values_t
{
    std::vector<std::string> values;
    std::unordered_map<std::string, std::string> options;
};

values_t get_values(const sol::object& obj)
{
    values_t values;

    if (obj.is<sol::table>()) {
        auto table = obj.as<sol::table>();
        for (auto&[key, value] : table) {
            if (key.is<sol::string_view>()) {
                values.options[key.as<std::string>()] = value.as<std::string>();
            }
        }
        int i = 1;
        while (table.get<sol::object>(i)) {
            auto object = table.get<sol::object>(i++);
            if (object.is<sol::string_view>()) {
                values.values.push_back(object.as<std::string>());
            } else {
                log_error("Illegal non-string argument in get_values");
            }
        }
    } else {
        values.values.push_back(obj.as<std::string>());
    }

    return values;
}

void populate_artifactory_from_file(project_artifactory_t& artifactory, const fs::path& file, flags_t flags)
{
    sol::state lua;

    lua.open_libraries(
        sol::lib::base,
        sol::lib::coroutine,
        sol::lib::string,
        sol::lib::io);

    auto default_dir = file.parent_path();

    project_t* project = nullptr;

    // TODO:
    lua.set_function("Os", []{});
    lua.set_function("Error", []{});

    lua.set_function("Project", [&](std::string_view name) {
        if (project && is_set(flags, flags_t::trace)) {
            debug_project(*project);
        }

        project = new project_t{};
        project->name = std::string(name);
        project->dir = default_dir;

        artifactory.projects.insert({ project->name, project });

        return true;
    });

    lua.set_function("Dir", [&](std::string_view name) {
        project->dir = {fs::absolute(default_dir / name).string()};
    });

    lua.set_function("Compile", [&](const sol::object& obj) {
        auto values = get_values(obj);
        source_type_t type = source_type_t::automatic;
        if (values.options.contains("type")) {
            auto type_str = values.options.at("type");
            if      (type_str == "cppm") type = source_type_t::cppm;
            else if (type_str == "cpp")  type = source_type_t::cpp;
            else if (type_str == "c")    type = source_type_t::c;
        }

        for (auto& value : values.values) {
            project->sources.push_back({ project->dir / value, type });
        }
    });

    lua.set_function("Include", [&](const sol::object& obj) {
        auto values = get_values(obj);
        for (auto& value : values.values) {
            auto path = project->dir / value;
            if (!fs::exists(path)) {
                // log_warn("Include path not found: {}", path.string());
            } else if (fs::is_directory(path)) {
                project->includes.push_back(project->dir / value);
            } else {
                project->force_includes.push_back(value);
            }
        }
    });

    lua.set_function("LibPath", [&](const sol::object& obj) {
        auto values = get_values(obj);
        for (auto& value : values.values) {
            project->lib_paths.push_back(project->dir / value);
        }
    });

    lua.set_function("Import", [&](const sol::object& obj) {
        auto values = get_values(obj);
        for (auto& value : values.values) {
            project->imports.emplace_back(std::move(value));
        }
    });

    lua.set_function("Define", [&](const sol::object& obj) {
        auto values = get_values(obj);
        bool build_scope = true;
        bool import_scope = true;
        if (values.options.contains("scope")) {
            auto scope = values.options.at("scope");
            if (scope == "build") {
                import_scope = false;
            }
            if (scope == "import") {
                build_scope = false;
            }
        }
        for (auto& define : values.values) {
            std::string key;
            std::string value;
            auto equals = define.find_first_of('=');
            if (equals != std::string::npos) {
                key = define.substr(0, equals);
                value = define.substr(equals + 1);
            } else {
                key = define;
            }
            if (build_scope)  project->build_defines.push_back({ key, value });
            if (import_scope) project->defines.push_back({ key, value });
        }
    });

    lua.set_function("Artifact", [&](const sol::object& obj) {
        auto values = get_values(obj);
        artifact_t artifact{};
        if (values.values.empty()) {
            log_error("Missing target path for Artifact");
        }
        artifact.path = project->dir / values.values.front();
        if (!values.options.contains("type")) {
            log_error("Missing type: Console/Window for Artifact");
        }
        auto& type = values.options.at("type");
        if      (type == "Console") { artifact.type = artifact_type_t::console;        }
        if      (type == "Window")  { artifact.type = artifact_type_t::window;         }
        else if (type == "Shared")  { artifact.type = artifact_type_t::shared_library; }

        project->artifact = std::move(artifact);
    });

    lua.set_function("Link", [&](const sol::object& obj) {
        auto values = get_values(obj);
        for (auto& value : values.values) {
            project->links.push_back(project->dir / value);
        }
    });

    lua.set_function("Shared", [&](const sol::object& obj) {
        auto values = get_values(obj);
        for (auto& value : values.values) {
            project->shared_libs.push_back(project->dir / value);
        }
    });

    sol::environment env(lua, sol::create, lua.globals());
    try {
        auto res = lua.safe_script_file(file.string(), &sol::script_throw_on_error);
        if (!res.valid()) {
            sol::error error = res;
            log_error("Error returned running bldr file: {}", error.what());
            std::exit(1);
        }
    } catch (const std::exception& e) {
        log_error("Exception thrown running bldr file: {}", e.what());
    } catch (...) {
        log_error("Unknown error thrown running bldr file");
    }

    if (project && is_set(flags, flags_t::trace)) {
        debug_project(*project);
    }
}

void populate_artifactory(project_artifactory_t& artifactory, flags_t flags)
{
    for (auto& file : fs::directory_iterator(fs::current_path())) {
        if (file.path().string().ends_with("bldr.lua")) {
            if (is_set(flags, flags_t::trace)) {
                log_debug("Loading bldr file: {}", file.path().string());
            }
            populate_artifactory_from_file(artifactory, file.path(), flags);
        }
    }

    std::unordered_set<fs::path> installed;
    {
        std::ifstream fs(s_paths.installed, std::ios::binary);
        if (fs.is_open()) {
            std::string line;
            while (std::getline(fs, line)) {
                installed.insert(line);
            }
        }
    }
    for (auto& file : installed) {
        if (fs::exists(file)) {
            if (is_set(flags, flags_t::trace)) {
                log_info("Loading installed bldr file: {}", file.string());
            }
            populate_artifactory_from_file(artifactory, file, flags);
        }
    }
}