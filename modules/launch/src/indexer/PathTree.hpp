#pragma once
#ifndef NMS_PATH_TREE_H
#define NMS_PATH_TREE_H

#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <optional>
#include <iostream>
#include <functional>

#include <algorithm>
#include <execution>

#include <format>
#include <span>

/**
 * @brief Represents a single file/folder within the file index
 * Size: 12 bytes (no padding)
 */
struct PathNode {
  uint32_t strBegin;
  uint32_t parent;
  uint8_t len;
  uint8_t depth;
  uint8_t match;
  uint8_t inheritedMatch;
};

class PathTree;

/**
 * @brief A wrapped view of a selected path. Generated from a PathNode
 */
class PathView {
  friend PathTree;

  std::filesystem::path _path;
  uint32_t index;

  PathView(std::filesystem::path path, uint32_t index) 
    : _path(path)
    , index(index) {
  }
public:

  const std::filesystem::path& path() const {
    return _path;
  }
};

/**
 * @brief A complete file system index tree
 */
class PathTree {
  friend PathView;
public:

  std::string data;
  std::string_view view;
  std::vector<PathNode> nodes;

  uint8_t matchBits = 0;
  bool useInheritMatch;

  PathTree()
    : data("")
    , view("")
    , nodes(std::vector<PathNode>(0)) {}

  /**
   * @brief Internal constructor
   * 
   * @param data 
   * @param nodes 
   */
  PathTree(std::string data, std::vector<PathNode> nodes)
    : data(data)
    , nodes(nodes) {  

    view = { data.begin(), data.end() };
  }

  ///////////////////////////
  // String representation //
  ///////////////////////////

  std::string nodePath(const PathNode& node) const {
    return data.substr(node.strBegin, node.len);
  }

  std::string fullPath(const PathNode& node, bool dir = false) const {
    const PathNode& parent = nodes[node.parent];
    std::string nodeStr = data.substr(node.strBegin, node.len);
    if (dir && (nodeStr.size() == 0 || nodeStr[nodeStr.size() - 1] != '\\'))
      nodeStr += '\\';
    return (parent.strBegin != node.strBegin)
      ? fullPath(parent, true) + nodeStr
      : nodeStr;
  }

  ////////////////////////////
  // Comparison and sorting //
  ////////////////////////////

  std::partial_ordering compareNames(const PathNode& p1, const PathNode& p2) const {
    return p1.len != p2.len
      ? (p1.len < p2.len ? std::partial_ordering::less : std::partial_ordering::greater)
      : view.substr(p1.strBegin, p1.len) <=> view.substr(p2.strBegin, p2.len);
  }

  std::partial_ordering comparePaths(const PathNode& p1, const PathNode& p2) const {
    if (p1.depth != p2.depth) {
      return p1.depth < p2.depth 
        ? std::partial_ordering::less 
        : std::partial_ordering::greater;
    }
    else if (p1.strBegin == p2.strBegin) {
      return std::partial_ordering::equivalent;
    }
    else if (p1.depth == 0) {
      return compareNames(p1, p2);
    } 
    else {
      std::partial_ordering order = comparePaths(nodes[p1.parent], nodes[p2.parent]);
      return order == std::partial_ordering::equivalent
        ? compareNames(p1, p2)
        : order;
    }
  }

  bool comparePathsLess(const PathNode& p1, const PathNode& p2) const {
    return comparePaths(p1, p2) == std::partial_ordering::less;
  }

  void sort() {
    // Fix view
    view = { data.begin(), data.end() };
    uint32_t size = nodes.size();

    // Create a temporary structure to remember the old index after sorting.
    struct SortNode {
      uint32_t old_index;
      PathNode node;
    };
    std::vector<SortNode> to_sort(size);

  #pragma omp parallel for
    for (long i = 0; i < size; ++i) {
      to_sort[i] = { static_cast<uint32_t>(i), nodes[i] };
    }

    // Sort temporary
    std::sort(std::execution::par, to_sort.begin(), to_sort.end(), 
      [&](const SortNode& p1, const SortNode& p2) {
        return comparePathsLess(p1.node, p2.node);
      }
    );

    // Compute mapping of old indexes to new indexes
    std::vector<uint32_t> old_to_new_indexes(size);
  #pragma omp parallel for
    for (long i = 0; i < size; ++i) {
      old_to_new_indexes[to_sort[i].old_index] = i;
    }

    // Copy nodes back into permanent structure, with correct parent indexes
  #pragma omp parallel for
    for (long i = 0; i < size; ++i) {
      PathNode& node = to_sort[i].node;
      nodes[i] = node;
      nodes[i].parent = old_to_new_indexes[node.parent];
    }
  }

  //////////////////////////////
  // Iteration and navigation //
  //////////////////////////////

  std::optional<PathView> makePathViewForNode(const PathNode& node, uint32_t index) const {
    std::vector<const PathNode*> parents{};
    parents.push_back(&node);
    auto* cur = &node;
    while (cur && (cur->strBegin != nodes[cur->parent].strBegin)) {
      cur = &nodes[cur->parent];
      parents.push_back(cur);
    }
    
    try {
      int i = parents.size() - 1;
      std::filesystem::path path = nodePath(*parents[i--]);
      while (i >= 0) path /= nodePath(*parents[i--]);

      return PathView { std::move(path), index };
    } catch (...) { }
    return std::nullopt;
  }

  std::optional<PathView> next(const PathView* current) {
    int64_t i = current ? current->index + 1 : 0;
    size_t max = nodes.size();
    while (i < max) {
      if (checkMatch(nodes[i], matchBits, useInheritMatch)) {
        auto path = makePathViewForNode(nodes[i], i);
        if (path) return path;
      }
      ++i;
    }

    return std::nullopt;
  }

  std::optional<PathView> prev(const PathView* current) {
    int64_t i = current ? current->index : nodes.size();
    i--;
    while (i >= 0) {
      if (checkMatch(nodes[i], matchBits, useInheritMatch)) {
        auto path = makePathViewForNode(nodes[i], i);
        if (path) return path;
      }
      --i;
    }

    return std::nullopt;
  }

  ////////////////////////////
  // Filtering and Matching //
  ////////////////////////////

  bool filter(const PathView& item) {
    return checkMatch(nodes[item.index], matchBits, useInheritMatch);
  }

  void setMatchBits(uint8_t mask, uint8_t value, uint8_t iMask, uint8_t iValue) {
    std::for_each(std::execution::par_unseq, nodes.begin(), nodes.end(), [&](PathNode& p) {
      p.match = (p.match & ~mask) | value;
      p.inheritedMatch = (p.inheritedMatch & ~iMask) | iValue;
    });
  }

  void matchLazy(uint8_t matchCheck, uint8_t matchBit, std::function<bool(std::string_view)>&& test) {
    view = { data.begin(), data.end() };
    std::for_each(std::execution::par_unseq, nodes.begin(), nodes.end(), [&](PathNode& p) {

      // checkMatch is faster but prevents you from typing keywords out of order
      // E.g. Windows C:\ finds no results because C:\ doesn't inherit from Windows
      // Possible solved by adding backwards inheritance? But don't want to increase
      // memory footprint
      
      // if (!checkMatch(p, matchCheck, useInheritMatch) || !test(view.substr(p.strBegin, p.len))) {
      if (((p.match & matchBit) == matchBit) && !test(view.substr(p.strBegin, p.len))) {
        p.match &= ~matchBit;
      }
    });
  }

  void match(uint8_t matchBit, std::function<bool(std::string_view)>&& test) {
    view = { data.begin(), data.end() };
    std::for_each(std::execution::par_unseq, nodes.begin(), nodes.end(), [&](PathNode& p) {
      p.match = test(view.substr(p.strBegin, p.len))
        ? p.match | matchBit 
        : p.match & ~matchBit;
    });
  }
  
  template<typename Iter>
  std::optional<PathView> matchComplete(const uint8_t match_bit, Iter iter, Iter last)
  {
    const std::string_view view = { data.begin(), data.end() };

    uint32_t m = nodes.size();
    uint32_t i = 0;
    uint32_t parent;

    int debug = 100;

    //  Find first value to use as parent for further matches
    while (i < m) {
      
      auto& p = nodes[i];
      if (debug-- > 0)
        std::cout << std::format("str = {}, needle = {}\n", view.substr(p.strBegin, p.len), *iter);
      if (view.substr(p.strBegin, p.len) == *iter) {
        // Found first element in path
        std::cout << std::format("Found first [{}] @ {}\n", *iter, fullPath(p));
        parent = i;
        iter++;
        if (iter == last) {
          //  Found only element in path
          p.match = 1;
          // return PathView { this, nodes[i], static_cast<uint32_t>(i) };
          return makePathViewForNode(nodes[i], i);
        }
        i++;
        break;
      }
      i++;
    }
    
    //  Now find rest of path
    while (i < m) {
      auto& p = nodes[i];
      
      if (debug-- > 0)
        std::cout << std::format("  substr = {}, needle = {}, sub = {}\n", view.substr(p.strBegin, p.len), *iter, parent == p.parent);
      if (parent == p.parent && view.substr(p.strBegin, p.len) == *iter) {  
        //  Found next element in path
        std::cout << std::format("Found [{}] @ {}\n", *iter, fullPath(p));
        parent = i;
        iter++;
        if (iter == last) {
          //  Found final element in path
          p.match = 1;  
          // return PathView { this, nodes[i], static_cast<uint32_t>(i) };
          return makePathViewForNode(nodes[i], i);
        }
        // match = (*iter).string();
      }
      i++;
    }

    std::cout << "Found nothing!\n";

    return std::nullopt;
  }

  void propogateMatches(const bool inheritanceChannel) {
    if (inheritanceChannel) {
      // for (auto& p : nodes) p.inheritedMatch = 0;
      for (auto& p : nodes) {
        auto& parent = nodes[p.parent];
        p.inheritedMatch = (parent.strBegin != p.strBegin) 
          ? parent.inheritedMatch | parent.match : 0;
        // p.inheritedMatch = nodes[p.parent].inheritedMatch | nodes[p.parent].match;
      }
    } else {
      for (auto& p : nodes) p.match |= nodes[p.parent].match;
    }
  }

  bool checkMatch(
      const PathNode& p, 
      const uint8_t matchBits, 
      const bool includeInherited) {
    return ((includeInherited ? (p.match | p.inheritedMatch) : p.match) & matchBits) == matchBits;
  }

  ///////////////////////////////////////
  // Serialization and Deserialization //
  ///////////////////////////////////////

  static PathTree load(const std::filesystem::path& file) {
    std::ifstream in(file, std::ios::in | std::ios::binary);
    if (in) {
      uint32_t pathCount;
      in.read(reinterpret_cast<char*>(&pathCount), sizeof(uint32_t));

      std::vector<PathNode> nodes(pathCount);
      in.read(reinterpret_cast<char*>(&nodes[0]), sizeof(PathNode) * pathCount);

      const auto dataStart = in.tellg();
      in.seekg(0, std::ios::end);
      const auto strBytes = in.tellg() - dataStart;
      in.seekg(dataStart, std::ios::beg);
      std::string data;
      data.resize(strBytes);
      in.read(&data[0], strBytes);

      in.close();

      return PathTree(data, nodes);
    }

    return PathTree("", std::vector<PathNode>(0));
  }

  void save(const std::filesystem::path& file) {
    std::ofstream out(file, std::ios::out | std::ios::binary);
    if (out) {
      const uint32_t pathCount = nodes.size();
      out.write(reinterpret_cast<const char*>(&pathCount), sizeof(uint32_t));
      out.write(reinterpret_cast<const char*>(&nodes[0]), sizeof(PathNode) * pathCount);
      out.write(&data[0], data.size());

      out.close();
    }
  }

  void compare_lengths() {
    std::unordered_map<uint32_t, uint32_t> len_counts;
    for (auto& n : nodes) {
      len_counts[n.len]++;
    }

    std::vector<std::pair<uint32_t, uint32_t>> results;
    for (auto& p : len_counts) results.push_back(p);

    std::sort(results.begin(), results.end(), [](auto& l, auto& r) { return l.second >= r.second; });
    for (auto& p : results) {
      std::cout << "Len " << p.first << " -> " << p.second << '\n';
    }
  }

  struct TreeNode {
    TreeNode* parent;
    std::string path;
    std::vector<std::unique_ptr<TreeNode>> children;

    size_t size() {
      size_t total_size = sizeof(TreeNode);
      total_size += path.length();
      for (auto& c : children) {
        total_size += 8; // pointer to child
        total_size += c->size(); // size of child
      }
      return total_size;
    }
  };

  void build_fulltree() {
    std::vector<std::unique_ptr<TreeNode>> roots;
    std::unordered_map<uint32_t, TreeNode*> graph;
    std::vector<TreeNode*> sorted;

    view = { data.begin(), data.end() };

    using namespace std::chrono;

    auto start = steady_clock::now();

    // for (auto& n : nodes) {
    for (size_t i = 0; i < nodes.size(); ++i) {
      auto& n = nodes[i];
      auto s = view.substr(n.strBegin, n.len);
      auto existing = graph[n.parent];
      if (!existing) {
        auto r = std::unique_ptr<TreeNode>(new TreeNode{nullptr, std::string(s)});
        graph[i] = r.get();
        sorted.push_back(r.get());
        roots.push_back(std::move(r));
      } else {
        auto r = std::unique_ptr<TreeNode>(new TreeNode{existing, std::string(s)});
        graph[i] = r.get();
        sorted.push_back(r.get());
        existing->children.push_back(std::move(r));
      }
    }

    std::cout << "Built tree in " << duration_cast<milliseconds>(steady_clock::now() - start) << '\n';

    std::cout << "num roots = " << roots.size() << '\n';
    std::cout << "count = " << sorted.size() << '\n';

    size_t total_size = 0;
    for (auto& root : roots) total_size += root->size();
    total_size += 8 * sorted.size();

    std::cout << "Used bytes = " << total_size << " (MB = " << (total_size / (1024 * 1024)) << ")\n";
  }

  //////////////////////
  // Index generation //
  //////////////////////

  template<typename Roots>
  static PathTree index(const Roots& roots, PathTree* prev = nullptr) {
    std::unordered_map<std::string, int> elements;
    std::string data;
    std::vector<PathNode> nodes;

    if (prev) {
      prev->view = { prev->data.begin(), prev->data.end() };

      prev->setMatchBits(0xFFc, 0, 0xFFc, 0);
      for (const auto& root : roots) {
        std::vector<std::string> components;
        for (auto& p : root) {
          auto s = p.string();
          if (s == "\\") continue;
          if (s.ends_with(":")) s.append("\\");
          components.push_back(std::move(s));
        }
        prev->matchComplete(1, components.begin(), components.end());
      }
      prev->propogateMatches(true);

      auto preserved = 0ull;
      auto dropped = 0ull;

      for (auto& n : prev->nodes) {
        if (!prev->checkMatch(n, 1, true)) {
          preserved++;
          auto parent_str = prev->fullPath(n, false);
          std::string name { prev->view.substr(n.strBegin, n.len) };
          uint32_t str_offset = data.size();
          data.append(name);

          auto index = nodes.size();
          auto search = elements.find(parent_str);
          uint32_t parent = search == elements.end() ? index : search->second;

          nodes.push_back(PathNode {
            str_offset,
            parent,
            static_cast<uint8_t>(name.size()),
            0,
            0
          });

          elements[std::string(name)] = index;
        } else {
          dropped++;
        }

        if ((preserved + dropped) % 10000 == 0) {
          std::cout << "  checked: " << (preserved + dropped) << '\n';
        }
      }

      std::cout << "Preserved " << preserved << " nodes!\n";
      std::cout << "Dropped" << dropped << " nodes!\n";
    }

    for (const std::filesystem::path& root : roots) {
      std::cout << std::format("Loading root [{}]\n", root.string());
      if (!std::filesystem::exists(root)) 
        return PathTree(std::move(data), std::move(nodes));

      {
        std::string rootString = root.string();
        const uint32_t strOffset = data.size();
        data.append(rootString);

        const uint32_t index = nodes.size();
        const std::string parentStr = root.parent_path().string();
        const auto search = elements.find(parentStr);
        const uint32_t parent = search == elements.end() ? index : search->second;

        nodes.push_back(PathNode { 
          strOffset, 
          parent, 
          static_cast<uint8_t>(rootString.size()), 
          0, 
          0 
        });

        elements[std::move(rootString)] = index;
      }

      namespace fs = std::filesystem;
      
      auto iter = fs::recursive_directory_iterator(
        root, fs::directory_options::skip_permission_denied 
          | fs::directory_options::follow_directory_symlink);
      const auto endIter = fs::end(iter);
      std::error_code ec;
      int failed = 0;

      for (; iter != endIter; iter.increment(ec)) {
        if (ec) {
          std::cerr << std::format("Error[{}] - {}\n", ++failed, ec.message());
          continue;
        }

        try {
          const auto& path = iter->path();

          const std::string parentStr = reinterpret_cast<const char*>(path.parent_path().u8string().c_str());
          const auto search = elements.find(parentStr);

          if (search == elements.end()) {
            // Only include nodes if we have stored its parent node.
            // Don't log - Cause of missing parent already logged or intentional
            ++failed;
            continue;
          }

          const std::string name = reinterpret_cast<const char*>(path.filename().u8string().c_str());
          if (name.size() > 255) {
            // len must be [0..255]
            std::cerr << std::format("Error[{}] - Name {} over 255 bytes!\n", ++failed, name);
            continue;
          }

          const std::string str = reinterpret_cast<const char*>(path.u8string().c_str());
          // Do all string covnersions early to ensure no encoding failures
          //   after data structures have been modified

          const uint32_t parent = search->second;
          PathNode& p = nodes[parent];
          const uint8_t depth = p.depth == 255 ? 255 : p.depth + 1;
          // Truncate depth at 255, sort will recurse until it finds a depths value < 255

          const uint32_t strOffset = data.size();
          data.append(name);

          const uint32_t index = nodes.size();
          nodes.push_back(PathNode {
            strOffset,
            parent,
            static_cast<uint8_t>(name.size()),
            depth,
            0,
            0
          });

          if (nodes.size() % 10'000 == 0) {
            std::cout << std::format("Files = {}\n", nodes.size());
          }

          elements[str] = index;
        }
        catch ([[maybe_unused]] const std::exception& e) {
          std::cerr << std::format("Error[{}] - Exception: {}\n", ++failed, e.what());
        }
      }
    }

    std::cout << "\nFiles = " << nodes.size() << '\n';
    std::cout << "Data len = " << data.size() << '\n';
    std::cout << "Index bytes = " << nodes.size() * sizeof(PathNode) << '\n';

    return PathTree(std::move(data), std::move(nodes));
  }
};

#endif // !NMS_PATH_TREE_H