#include "bldr.hpp"
#include <log.hpp>

std::ostream& operator<<(std::ostream& os, source_type_t type)
{
    switch (type)
    {
        using enum source_type_t;
        case automatic: return os << "automatic";
        case cppm:      return os << "cppm";
        case cpp:       return os << "cpp";
        case c:         return os << "c";
        default:        return os << "invalid";
    }
}

std::ostream& operator<<(std::ostream& os, const define_t& define)
{
    return os << define.key << " = " << define.value;
};

std::ostream& operator<<(std::ostream& os, const source_t& source)
{
    return os << source.file << " [" << source.type << "]";
}

std::ostream& operator<<(std::ostream& os, artifact_type_t type)
{
    switch (type)
    {
        using enum artifact_type_t;
        case console:        return os << "console";
        case window:         return os << "window";
        case shared_library: return os << "shared_library";
        default:             return os << "invalid";
    }
}

std::ostream& operator<<(std::ostream& os, const artifact_t& artifact)
{
    return os << artifact.path << " [" << artifact.type << "]";
}

void debug_project(project_t& project)
{
    log_debug("Project: {}", project.name);
    log("  dir = {}", project.dir.string());

    auto print_all = [&](std::string_view name, auto& list) {
        if (list.empty()) return;
        std::cout << "  " << name << ":\n";
        for (auto& entry : list) {
            std::cout << "   - " << entry << '\n';
        }
    };

    print_all("sources",        project.sources);
    print_all("includes",       project.includes);
    print_all("force_includes", project.force_includes);
    print_all("lib_paths",      project.lib_paths);
    print_all("imports",        project.imports);
    print_all("links",          project.links);
    print_all("build_defines",  project.build_defines);
    print_all("defines",        project.defines);

    if (project.artifact) {
        std::cout << "  artifact: " << project.artifact.value() << '\n';
    }
}