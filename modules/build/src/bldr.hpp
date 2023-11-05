#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <filesystem>
#include <span>

using namespace std::literals;
namespace fs = std::filesystem;

struct paths_t
{
    fs::path dir;
    fs::path installed;
    fs::path artifacts;
    fs::path environments;
};

inline paths_t s_paths;

enum class flags_t : uint64_t
{
    none,
    clean  = 1 << 0,
    nowarn = 1 << 1,
    noopt  = 1 << 2,
};

inline
bool is_set(flags_t flags, flags_t test)
{
    return std::to_underlying(flags) & std::to_underlying(test);
}

inline
flags_t operator|(flags_t l, flags_t r)
{
    return flags_t(std::to_underlying(l) | std::to_underlying(r));
}

enum class source_type_t
{
    automatic,
    cppm,
    cpp,
    c,
    _max_enum,
};

inline
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

struct path_t
{
    std::string path;
    path_t*   parent = nullptr;

    fs::path to_fspath(int end_cutoff = 0) const
    {
        auto head = std::string_view(path).substr(0, path.size() - end_cutoff);
        if (parent) return parent->to_fspath() / head;
        else return head;
    }
};

struct define_t
{
    std::string key;
    std::string value;
};

struct source_t
{
    path_t        file;
    source_type_t type;
};


enum class artifact_type_t
{
    _invalid,
    console,
    window,
    shared_library,
    _max_enum,
};

struct artifact_t
{
    path_t          path;
    artifact_type_t type;
};

struct project_t
{
    std::string name;
    path_t      dir;

    std::vector<source_t>    sources;
    std::vector<path_t>      includes;
    std::vector<path_t>      force_includes;
    std::vector<path_t>      lib_paths;
    std::vector<std::string> imports;
    std::vector<path_t>      links;
    std::vector<define_t>    build_defines;
    std::vector<define_t>    defines;

    std::optional<artifact_t> artifact;
};

struct project_artifactory_t
{
    std::unordered_map<std::string_view, project_t*> projects;
};

void populate_artifactory(project_artifactory_t& artifactory);
void generate_build(project_artifactory_t& artifactory,  project_t& project, project_t& output);
void debug_project(project_t& project);
void build_project(project_t& project, flags_t flags);
void configure_ide(project_t& prjoect, flags_t flags);

struct env_variable_t
{
    std::string key;
    std::string value;
};

struct environment_t
{
    std::vector<env_variable_t> variables;
};

struct program_exec_t
{
    path_t working_directory;

    program_exec_t* parent = nullptr;

    environment_t*           environment = nullptr;
    std::vector<std::string> arguments;

};

void execute_program(const program_exec_t& info);