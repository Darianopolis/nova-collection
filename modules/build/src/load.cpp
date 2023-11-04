#include "bldr.hpp"

#include <sol/sol.hpp>

struct values_t
{
    std::vector<std::string> values;
    std::unordered_map<std::string, std::string> options;
};

values_t get_values(sol::object obj)
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
        while (table[i]) {
            values.values.push_back(table[i++].get<std::string>());
        }
    } else {
        values.values.push_back(obj.as<std::string>());
    }

    return values;
}

void populate_artifactory_from_file(project_artifactory_t& artifactory, const std::string& file)
{
    sol::state lua;

    lua.open_libraries(
        sol::lib::base,
        sol::lib::coroutine,
        sol::lib::string,
        sol::lib::io);

    project_t* project = nullptr;

    lua.set_function("Project", [&](std::string_view name) {
        if (project) {
            debug_project(*project);
        }

        project = new project_t{};
        project->name = std::string(name);
        project->dir = path_t(std::filesystem::absolute(std::filesystem::current_path()).string());

        artifactory.projects.insert({ project->name, project });

        return true;
    });

    lua.set_function("Dir", [&](std::string_view name) {
        project->dir = {std::filesystem::absolute(name).string()};
    });

    lua.set_function("Compile", [&](sol::object obj) {
        auto values = get_values(obj);
        source_type_t type = source_type_t::automatic;
        if (values.options.contains("type")) {
            auto type_str = values.options.at("type");
            if      (type_str == "cppm") type = source_type_t::cppm;
            else if (type_str == "cpp")  type = source_type_t::cpp;
            else if (type_str == "c")    type = source_type_t::c;
        }

        for (auto& value : values.values) {
            project->sources.push_back({ {std::move(value), &project->dir}, type });
        }
    });

    lua.set_function("Include", [&](sol::object obj) {
        auto values = get_values(obj);
        for (auto& value : values.values) {
            project->includes.push_back({ std::move(value), &project->dir });
        }
    });

    lua.set_function("Import", [&](sol::object obj) {
        auto values = get_values(obj);
        for (auto& value : values.values) {
            project->imports.emplace_back(std::move(value));
        }
    });

    lua.set_function("Artifact", [&](sol::object obj) {
        auto values = get_values(obj);
        artifact_t artifact{};
        artifact.path = {values.values.front(), &project->dir};
        auto& type = values.options.at("type");
        if      (type == "Console") { artifact.type = artifact_type_t::executable;     artifact.path.path += ".exe"; }
        else if (type == "Shared")  { artifact.type = artifact_type_t::shared_library; artifact.path.path += ".dll"; }

        project->artifact = std::move(artifact);
    });

    lua.set_function("Link", [&](sol::object obj) {
        auto values = get_values(obj);
        for (auto& value : values.values) {
            project->links.push_back({std::string(value), &project->dir});
        }
    });

    sol::environment env(lua, sol::create, lua.globals());
    lua.script_file(file);

    if (project) {
        debug_project(*project);
    }
}

void populate_artifactory(project_artifactory_t& artifactory)
{
    populate_artifactory_from_file(artifactory, "global.bldr.lua");
    populate_artifactory_from_file(artifactory, "bldr.lua");
}