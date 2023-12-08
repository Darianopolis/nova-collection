#include "bldr.hpp"
#include "log.hpp"

#include <fstream>
#include <unordered_set>
#include <format>

void display_help(std::string_view message = {})
{
    if (!message.empty()) {
        log_error("{}", message);
    }

    log_info(R"(Usage:
bldr install add    : Install new bldr files
     .       remove : Uninstall bldr files
     .       .        -all : Delete *all* files
     .       clean  : Clean broken bldr files
     .       list   : List bldr files
     env     clear  : Clear environemnts
     make           : Build projects
     .                -clean   : Clean build
     .                -no-warn : Disable warnings
     .                -no-opt  : Disable optimizations
     ide            : Configure intellisense for supported IDEs
)");
    std::exit(1);
}

int main(int argc, char* argv[]) try
{
    std::vector<std::string_view> args(argv, argv + argc);

    s_paths.dir = fs::path(std::getenv("USERPROFILE")) / ".bldr";
    s_paths.artifacts    = s_paths.dir / "artifacts";
    s_paths.environments = s_paths.dir / "environments";
    s_paths.installed    = s_paths.dir / "installed";
    fs::create_directories(s_paths.dir);

    if (args.size() < 2) display_help("Expected action");

    if (args[1] == "install") {
        if (args.size() < 3) display_help("Expected argument after install");

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

        if (args[2] == "add") {
            for (uint32_t i = 3; i < args.size(); ++i ){
                auto path = fs::absolute(fs::path(args[i]));
                if (!fs::exists(path)) {
                    log_warn("{} does not exist, skipping!", path.string());
                    continue;
                }
                log_info("Adding: {}", path.string());
                installed.insert(path);
            }

        } else if (args[2] == "remove") {
            if (args.size() > 3 && args[3] == "-all") {
                installed.clear();
            } else {
                for (uint32_t i = 3; i < args.size(); ++i ) {
                    auto path = fs::absolute(fs::path(args[i]));
                    if (installed.contains(path)) {
                        log_info("Removing: {}", path.string());
                        installed.erase(path);
                    }
                }
            }

        } else if (args[2] == "clean") {
            std::vector<fs::path> to_delete;
            for (auto& file : installed) {
                if (!fs::exists(file)) {
                    to_delete.push_back(file);
                }
            }
            for (auto& del : to_delete) {
                log_info("Removing: {}", del.string());
                installed.erase(del);
            }

        } else if (args[2] == "list") {
            log_info("Installed files:");
            for (auto& file : installed) {
                log(" - {}", file.string());
            }
        } else {
            display_help(std::format("Expected [add/remove/clean/list] after 'install', but got '{}'", args[2]));
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

    } else if (args[1] == "make" || args[1] == "ide") {
        std::vector<std::string_view> projects;
        flags_t flags{};
        for (uint32_t i = 2; i < args.size(); ++i) {
            auto& arg = args[i];
            if      (arg == "-clean")   flags = flags | flags_t::clean;
            else if (arg == "-no-warn") flags = flags | flags_t::nowarn;
            else if (arg == "-no-opt")  flags = flags | flags_t::noopt;
            else if (arg == "-trace")   flags = flags | flags_t::trace;
            else projects.push_back(arg);
        }

        project_artifactory_t artifactory;
        populate_artifactory(artifactory, flags);

        std::vector<project_t*> to_build;

        for (auto& name : projects) {
            if (!artifactory.projects.contains(name)) {
                log_warn("Could not find project with name [{}]", name);
            }
            auto* build = new project_t();
            generate_build(artifactory, *artifactory.projects.at(name), *build);
            if (is_set(flags, flags_t::trace)) {
                log_debug(" ---- Combined project ----");
                debug_project(*build);
            }
            if      (args[1] == "make") to_build.push_back(build);
            else if (args[1] == "ide")  configure_ide(*build, flags);
        }

        if (to_build.size()) {
            build_project(to_build, flags);
        }

    } else if (args[1] == "pack") {
        if (argc < 4) {
            display_help("Expected source and target for packing");
        }

        auto source = fs::path(args[2]);
        auto target = args[3];
        auto target_path = fs::path(target);
        target_path.replace_extension(".h");
        log_info("Packing {} into {}", source.string(), target);

        {
            std::ifstream in(source, std::ios::binary | std::ios::ate);
            std::vector<uint64_t> data;
            size_t size = in.tellg();
            data.resize((size + 7) / 8);
            in.seekg(0);
            in.read((char*)data.data(), size);

            std::ofstream out(target_path, std::ios::binary);
            out << "#include <stdint.h>\n";
            out << std::format("constexpr inline uint64_t {}[{}] = {{\n    ", target, data.size());
            for (size_t i = 0; i < data.size(); ++i) {
                if (i > 0) out << ",";
                out << std::format("{:#x}", data[i]);
            }
            out << "\n};\n";
            out << std::format("constexpr inline size_t {}_size = {};\n", target, size);
        }

    } else {
        display_help(std::format("Unknown action: '{}'", args[1]));
    }
}
catch (const std::exception& e)
{
    log_error("Caught: {}", e.what());
}
catch (...)
{
    log_error("Caught unknown error");
}