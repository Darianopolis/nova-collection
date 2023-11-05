#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <filesystem>
#include <span>
#include <chrono>

using namespace std::literals;
namespace fs = std::filesystem;

// -----------------------------------------------------------------------------

struct paths_t
{
    fs::path dir;
    fs::path installed;
    fs::path artifacts;
    fs::path environments;
};

inline paths_t s_paths;

// -----------------------------------------------------------------------------

enum class flags_t : uint64_t
{
    none,
    clean  = 1 << 0,
    nowarn = 1 << 1,
    noopt  = 1 << 2,
    trace  = 1 << 3,
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

// -----------------------------------------------------------------------------

enum class source_type_t
{
    automatic,
    cppm,
    cpp,
    c,
    _max_enum,
};
std::ostream& operator<<(std::ostream& os, source_type_t type);

// -----------------------------------------------------------------------------

std::vector<fs::path> resolve_glob(fs::path path);

// -----------------------------------------------------------------------------

struct define_t
{
    std::string key;
    std::string value;
};

struct source_t
{
    fs::path      file;
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
    fs::path        path;
    artifact_type_t type;
};

struct project_t
{
    std::string name;
    fs::path    dir;

    std::vector<source_t>    sources;
    std::vector<fs::path>    includes;
    std::vector<fs::path>    force_includes;
    std::vector<fs::path>    lib_paths;
    std::vector<std::string> imports;
    std::vector<fs::path>    links;
    std::vector<define_t>    build_defines;
    std::vector<define_t>    defines;
    std::vector<fs::path>    shared_libs;

    std::optional<artifact_t> artifact;
};

struct project_artifactory_t
{
    std::unordered_map<std::string_view, project_t*> projects;
};

void populate_artifactory(project_artifactory_t& artifactory, flags_t flags);
void generate_build(project_artifactory_t& artifactory,  project_t& project, project_t& output);
void debug_project(project_t& project);
void build_project(std::span<project_t*> projects, flags_t flags);
void configure_ide(project_t& prjoect, flags_t flags);

// -----------------------------------------------------------------------------

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
    fs::path working_directory;

    program_exec_t* parent = nullptr;

    environment_t*           environment = nullptr;
    std::vector<std::string> arguments;

};

uint32_t execute_program(const program_exec_t& info, flags_t flags);