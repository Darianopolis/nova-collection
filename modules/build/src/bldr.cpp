#include "bldr.hpp"

#include <fstream>
#include <unordered_set>
#include <format>

void display_help(std::string_view message = {})
{
    if (!message.empty()) {
        std::cout << "Error: " << message << '\n';
    }

    std::cout << R"(
Usage:
bldr install add    : Install new bldr files
     .       remove : Uninstall bldr files
     .       .        -all : Delete *all* files
     .       clean  : Clean broken bldr files
     .       list   : List bldr files
     env     clear  : Clear environemnts
     make           : Build projects
                      -clean   : Clean build
                      -no-warn : Disable warnings
                      -no-opt  : Disable optimizations
)";
    std::exit(1);
}

int main(int argc, char* argv[]) try
{
    s_paths.dir = fs::path(std::getenv("USERPROFILE")) / ".bldr-cpp";
    s_paths.artifacts    = s_paths.dir / "artifacts";
    s_paths.environments = s_paths.dir / "environments";
    s_paths.installed    = s_paths.dir / "installed";
    fs::create_directories(s_paths.dir);

    std::vector<std::string_view> args(argv, argv + argc);
    if (args.size() < 2) display_help("Expected action");

    if (args[1] == "install") {
        if (args.size() < 3) display_help("Expected argument after install");

        std::unordered_set<std::filesystem::path> installed;
        {
            std::ifstream fs(s_paths.installed, std::ios::binary);
            if (fs.is_open()) {
                std::string line;
                while (std::getline(fs, line)) {
                    installed.insert(line);
                }
            }
        }

        if (args[2] == "add") {
            for (uint32_t i = 3; i < args.size(); ++i ){
                auto path = fs::absolute(fs::path(args[i]));
                if (!fs::exists(path)) {
                    std::cout << "bldr : " << path.string() << " does not exist, skipping!\n";
                    continue;
                }
                std::cout << "Adding: " << path.string() << '\n';
                installed.insert(path);
            }

        } else if (args[2] == "remove") {
            if (args.size() > 3 && args[3] == "-all") {
                installed.clear();
            } else {
                for (uint32_t i = 3; i < args.size(); ++i ) {
                    auto path = fs::absolute(fs::path(args[i]));
                    if (installed.contains(path)) {
                        std::cout << "Removing: " << path.string() << '\n';
                        installed.erase(path);
                    }
                }
            }

        } else if (args[2] == "clean") {
            std::vector<std::filesystem::path> to_delete;
            for (auto& file : installed) {
                if (!fs::exists(file)) {
                    to_delete.push_back(file);
                }
            }
            for (auto& del : to_delete) {
                std::cout << "Removing: " << del.string() << '\n';
                installed.erase(del);
            }

        } else if (args[2] == "list") {
            for (auto& file : installed) {
                std::cout << " - " << file.string() << '\n';
            }
        }

        {
            std::ofstream os(s_paths.installed, std::ios::binary);
            for (auto& file : installed) {
                os << file.string() << '\n';
            }
        }

    } else if (args[1] == "env") {
        if (args.size() < 3 || args[2] != "clear") display_help("Expected action after 'env'");

        fs::remove_all(s_paths.environments);

    } else if (args[1] == "make") {
        std::vector<std::string_view> projects;
        flags_t flags{};
        for (uint32_t i = 2; i < args.size(); ++i) {
            auto& arg = args[i];
            if      (arg == "-clean")   flags = flags | flags_t::clean;
            else if (arg == "-no-warn") flags = flags | flags_t::clean;
            else if (arg == "-no-opt")  flags = flags | flags_t::clean;
            else projects.push_back(arg);
        }

        project_artifactory_t artifactory;
        populate_artifactory(artifactory);

        for (auto& name : projects) {
            if (!artifactory.projects.contains(name)) {
                std::cout << "Could not find project with name [" << name << "]\n";
            }
            project_t build;
            generate_build(artifactory, *artifactory.projects.at(name), build);
            debug_project(build);
            build_project(build, flags);
        }
    } else {
        display_help(std::format("Unknown action: '{}'", args[1]));
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