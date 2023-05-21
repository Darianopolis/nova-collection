#define NOMINMAX
#include "Windows.h"

#include "NoMoreShortcuts.hpp"
#include <filesystem>

int main(int argc, char** argv) {
  std::cout << "Indexing:\n";
  // std::vector<std::filesystem::path> roots { "C:\\", "D:\\" };

  // std::vector<std::filesystem::path> roots;
  // for (size_t i = 1; i < argc; ++i) {
  //   std::cout << std::format("- {}\n", argv[i]);
  //   roots.emplace_back(argv[i]);
  // }
  // if (roots.empty()) {
  //   std::cout << "No root specified!\n";
  //   return 1;
  // }

  // namespace fs = std::filesystem;
  
  // auto file = std::filesystem::path(getenv("USERPROFILE") + std::string("\\.nms\\tree.bin"));
  // std::cout << std::format("File: {}\n", file.string());

  // auto tree_old = PathTree::load(file);
  // // tree_old.compare_lengths();
  // tree_old.build_fulltree();

  // // auto tree = PathTree::index(roots, &tree_old);
  // // tree.sort();
  // // tree.save(file);

  // return 0;

  std::vector<char> drives;
  for (size_t i = 1; i < argc; ++i) {
    drives.push_back(argv[i][0]);
  }
  if (drives.empty()) {
    std::cout << "Error, no drives selected!\n";
  }
  for (auto& d : drives) {
    auto *node = index_drive(d);
    // node->save(getenv("USERPROFILE") + std::string("\\.nms\\tree.bin"))
    auto saveLoc = std::format("{}\\.nms\\{}.index", getenv("USERPROFILE"), d);
    std::cout << std::format("Saving to {}\n", saveLoc);
    node->save(saveLoc);
  }
}
