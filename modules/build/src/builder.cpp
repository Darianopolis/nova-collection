#include "bldr.hpp"

#include <unordered_set>
#include <filesystem>

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

    output.artifact = project.artifact;
}