#pragma once
#ifndef FS_INDEX_HPP
#define FS_INDEX_HPP

#include <iostream>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <execution>

#include "UnicodeCollator.hpp"

static size_t nodes_created = 0;
static size_t nodes_destroyed = 0;

struct Node;

struct NodeIteratorRef {
  Node* node;
  size_t index;

  bool operator ==(const NodeIteratorRef& other) const noexcept {
    return node == other.node && index == other.index;
  };
};

struct NodeIterator {

  bool visit = false;
  std::vector<NodeIteratorRef> nodes;

  NodeIterator(Node* initial, size_t index);
  NodeIterator(const NodeIterator&);
  Node& operator *() const noexcept;
  Node* operator ->() const noexcept;
  NodeIterator& operator ++();
  friend bool operator ==(const NodeIterator &l, const NodeIterator& r);
};

// ---------------------------------------- //

struct NodeView {
  Node* node;
  // char* name;
  // uint8_t len;
  bool match = true;

  // NodeView(Node* node);
};

struct NodeFlat {
  uint32_t str_offset;
  uint32_t parent;
  uint8_t depth;
  uint8_t len;
  uint8_t match;
  uint8_t inherited_match;
};

struct NodeIndex {
  std::vector<NodeFlat> nodes;
  std::string str;
};

// ---------------------------------------- //

struct Node {
  char *name = nullptr;
  Node *parent = nullptr;
  Node **children = nullptr;
  uint32_t index = 0;
  uint32_t n_children = 0;
  // std::vector<std::unique_ptr<Node>> children;
  uint8_t len = 0;
  uint8_t depth;
  // uint16_t match = 0;

  // Node() {}
  Node(char *name, uint8_t len, Node *parent, uint8_t depth)
    : name(name), len(len), parent(parent), depth(depth) {
    nodes_created++;
  }

  void add_child(Node *child) {

    if (!n_children) {
      children = new Node*[2];
    } else if ((n_children & (n_children - 1)) == 0) {
      Node **new_children = new Node*[n_children * 2];
      memcpy(new_children, children, sizeof(Node*) * n_children);
      delete children;
      children = new_children;
    }
    children[n_children++] = child;

    // children.emplace_back(child);
  }

  ~Node() {
    nodes_destroyed++;
    if (name) free(name);
    if (children) {
      for (size_t i = 0; i < n_children; ++i)
        delete children[i];
      delete children;
    }
  }

  size_t count() {
    // size_t total = sizeof(Node);
    // total += len;
    // for (size_t i = 0; i < n_children; ++i) {
    //   total += sizeof(Node*) + children[i]->count();
    // }
    size_t total = 1;
    for (size_t i = 0; i < n_children; ++i) 
      total += children[i]->count();
    // for (auto& c : children)
    //   total += c->count();
    return total;
  }

  void save(std::ofstream& os) {
    os.write(reinterpret_cast<const char*>(&len), sizeof(uint8_t));
    os.write(name, len);
    os.write(reinterpret_cast<const char*>(&n_children), sizeof(uint32_t));
    for (size_t i = 0; i < n_children; ++i) {
      children[i]->save(os);
    }
  }

  void save(const std::filesystem::path& path) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream os(path, std::ios::out | std::ios::binary);
    if (os) {
      save(os);
      os.close();
    }
  }

  static Node* load(std::ifstream& is, Node *parent, uint8_t depth) {
    uint8_t len;
    is.read(reinterpret_cast<char*>(&len), 1);

    char *chars = new char[len + 1];
    is.read(chars, len);
    chars[len] = '\0';

    // size_t offset = vec.size();
    // char *chars = (char*)offset;
    // vec.resize(vec.size() + len + 1);
    // is.read(&vec[offset], len);
    // vec[offset + len] = '\0';

    uint32_t n_children;
    is.read(reinterpret_cast<char*>(&n_children), 4);

    Node *node = new Node(chars, len, parent, depth);
    for (size_t i = 0; i < n_children; ++i)
      node->add_child(load(is, node, depth == 255 ? 255 : depth + 1));

    return node;
  }

  static Node* load(std::filesystem::path path) {
    std::ifstream is(path, std::ios::in | std::ios::binary);
    return is ? load(is, nullptr, 0) : nullptr;
  }

  NodeIterator begin() {
    return NodeIterator(this, 0);
  }

  NodeIterator end() {
    return NodeIterator(this, n_children);
  }

  std::string string() {
    if (!parent) return name;
    std::string parent_str = parent->string();
    if (!parent_str.ends_with('\\')) parent_str += '\\';
    parent_str.append(name);
    return std::move(parent_str);
  }

  template<class Fn>
  void for_each(const Fn& fn) {
    fn(*this);
    for (size_t i = 0; i < n_children; ++i) {
      children[i]->for_each(fn);
    }
  }

  static std::weak_ordering cmp_len_lex(const Node& l, const Node& r) {
    using order = std::weak_ordering;

    return l.len != r.len
      ? (l.len < r.len ? order::less : order::greater)
      : (l.name <=> r.name);
  }

  static std::weak_ordering cmp_depth_len_lex(const Node& l, const Node& r) {
    using order = std::weak_ordering;

    if (l.depth != r.depth) {
      return l.depth < r.depth ? order::less : order::greater;
    } else if (l.name == r.name) {
      return order::equivalent;
    } else if (l.depth == 0) {
      return cmp_len_lex(l, r);
    } else {
      auto o = cmp_depth_len_lex(*l.parent, *r.parent);
      return o == order::equivalent ? cmp_len_lex(l, r) : o;
    }
  }
};

Node* index_drive(char drive);

NodeIndex flatten(std::vector<NodeView> nodes);

#endif // !DARIANOPOLIS_INDEX_HPP