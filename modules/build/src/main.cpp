#include <bldr.hpp>

int main() try
{
    project_t* luajit = new project_t {
        .name = "luajit",
        .dir = {"D:/Dev/Cloned/luajit"},
    };
    luajit->includes.push_back({"src", &luajit->dir});
    luajit->links.push_back({"src/luajit.lib", &luajit->dir});
    luajit->links.push_back({"src/lua51.lib", &luajit->dir});

    project_t* sol = new project_t {
        .name = "sol",
        .dir = {"D:/Dev/Cloned/sol2"},
    };
    sol->includes.push_back({"include", &sol->dir});

    project_t* bldr = new project_t {
        .name = "bldr",
        .dir  = {"D:/Dev/Projects/bldr-cpp"},
        .imports = { "luajit", "sol" },
    };
    bldr->sources.push_back({"src/**", &bldr->dir});
    bldr->includes.push_back({"src", &bldr->dir});
    bldr->artifact = {{"out/main.exe", &bldr->dir}, artifact_type_t::executable};

    project_artifactory_t artifactory;
    artifactory.projects.insert({ luajit->name, luajit });
    artifactory.projects.insert({ sol->name,    sol });
    artifactory.projects.insert({ bldr->name,   bldr });

    debug_project(*bldr);

    project_t combined{ .name = "bldr" };
    generate_build(artifactory, *bldr, combined);

    debug_project(combined);

    build_project(combined);
}
catch (const std::exception& e)
{
    std::cout << "Errror: " << e.what() << '\n';
}
catch (...)
{
    std::cout << "Error\n";
}