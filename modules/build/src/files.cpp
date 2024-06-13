#include "bldr.hpp"

std::vector<fs::path> resolve_glob(fs::path path)
{
    std::vector<fs::path> out;

    auto file_str = path.filename().string();
    auto i1 = file_str.find_first_of("*");
    auto i2 = file_str.find_last_of("*");

    if (i1 == std::string::npos) {
        out.push_back(std::move(path));
    } else {
        auto prefix = std::string_view(file_str).substr(0, i1);
        auto suffix = std::string_view(file_str).substr(i2 + 1);
        auto iterate = [&](auto&& iterator) {
            for (auto& entry : iterator) {
                auto cur_file = entry.path().filename().string();
                if (cur_file.size() < prefix.size() + suffix.size()) continue;
                if (!cur_file.starts_with(prefix)) continue;
                if (!cur_file.ends_with(suffix)) continue;
                out.push_back(std::move(entry.path()));
            }
        };
        if (fs::exists(path.parent_path())) {
            if (i1 == i2) iterate(fs::directory_iterator(path.parent_path()));
            else          iterate(fs::recursive_directory_iterator(path.parent_path()));
        }
    }

    return out;
}