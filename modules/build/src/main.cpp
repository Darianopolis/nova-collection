#include <bldr.hpp>

int main(int argc, char* argv[]) try
{
    project_artifactory_t artifactory;
    populate_artifactory(artifactory);

    std::vector<std::string_view> args(argv + 1, argv + argc);
    for (auto& name : args) {
        if (!artifactory.projects.contains(name)) {
            std::cout << "Could not find project with name [" << name << "]\n";
        }
        project_t build;
        generate_build(artifactory, *artifactory.projects.at(name), build);
        debug_project(build);
        build_project(build);
    }
}
catch (const std::exception& e)
{
    std::cout << "Errror: " << e.what() << '\n';
}
catch (...)
{
    std::cout << "Error\n";
}