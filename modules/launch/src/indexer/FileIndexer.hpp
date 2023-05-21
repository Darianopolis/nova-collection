#pragma once

#include "UnicodeCollator.hpp"

#include <iostream>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <execution>

static size_t nodesCreated = 0;
static size_t nodesDestroyed = 0;

struct Node;

struct NodeIteratorRef
{
    Node* node;
    size_t index;

    bool operator==(const NodeIteratorRef& other) const noexcept
    {
        return node == other.node && index == other.index;
    };
};

struct NodeIterator
{

    bool visit = false;
    std::vector<NodeIteratorRef> nodes;

    NodeIterator(Node* initial, size_t index);
    NodeIterator(const NodeIterator&);
    Node& operator*() const noexcept;
    Node* operator->() const noexcept;
    NodeIterator& operator++();
    friend bool operator==(const NodeIterator &l, const NodeIterator& r);
};

// ---------------------------------------- //

struct NodeView
{
    Node* node;
    // char* name;
    // uint8_t len;
    bool match = true;

    // NodeView(Node* node);
};

struct NodeFlat
{
    uint32_t strOffset;
    uint32_t parent;
    uint8_t depth;
    uint8_t len;
    uint8_t match;
    uint8_t inheritedMatch;
};

struct NodeIndex
{
    std::vector<NodeFlat> nodes;
    std::string str;
};

// ---------------------------------------- //

struct Node
{
    char* name = nullptr;
    Node* parent = nullptr;
    Node** children = nullptr;
    uint32_t index = 0;
    uint32_t numChildren = 0;
    // std::vector<std::unique_ptr<Node>> children;
    uint8_t len = 0;
    uint8_t depth;
    // uint16_t match = 0;

    // Node() {}
    Node(char* _name, uint8_t _len, Node* _parent, uint8_t _depth)
        : name(_name)
        , len(_len)
        , parent(_parent)
        , depth(_depth)
    {
        nodesCreated++;
    }

    void AddChild(Node* child)
    {

        if (!numChildren)
        {
            children = new Node*[2];
        }
        else if ((numChildren & (numChildren - 1)) == 0)
        {
            Node** new_children = new Node*[numChildren * 2];
            memcpy(new_children, children, sizeof(Node*) * numChildren);
            delete children;
            children = new_children;
        }

        children[numChildren++] = child;

        // children.emplace_back(child);
    }

    ~Node()
    {
        nodesDestroyed++;
        if (name)
            free(name);

        if (children)
        {
            for (size_t i = 0; i < numChildren; ++i)
                delete children[i];

            delete children;
        }
    }

    size_t Count()
    {
        // size_t total = sizeof(Node);
        // total += len;
        // for (size_t i = 0; i < n_children; ++i) {
        //   total += sizeof(Node*) + children[i]->count();
        // }
        size_t total = 1;
        for (size_t i = 0; i < numChildren; ++i)
            total += children[i]->Count();
        // for (auto& c : children)
        //   total += c->count();
        return total;
    }

    void save(std::ofstream& os)
    {
        os.write(reinterpret_cast<const char*>(&len), sizeof(uint8_t));
        os.write(name, len);
        os.write(reinterpret_cast<const char*>(&numChildren), sizeof(uint32_t));

        for (size_t i = 0; i < numChildren; ++i)
            children[i]->save(os);
    }

    void Save(const std::filesystem::path& path)
    {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream os(path, std::ios::out | std::ios::binary);

        if (os)
        {
            save(os);
            os.close();
        }
    }

    static Node* Load(std::ifstream& is, Node* parent, uint8_t depth)
    {
        uint8_t len;
        is.read(reinterpret_cast<char*>(&len), 1);

        char* chars = new char[len + 1];
        is.read(chars, len);
        chars[len] = '\0';

        // size_t offset = vec.size();
        // char* chars = (char*)offset;
        // vec.resize(vec.size() + len + 1);
        // is.read(&vec[offset], len);
        // vec[offset + len] = '\0';

        uint32_t numChildren;
        is.read(reinterpret_cast<char*>(&numChildren), 4);

        Node* node = new Node(chars, len, parent, depth);
        for (size_t i = 0; i < numChildren; ++i)
            node->AddChild(Load(is, node, depth == 255 ? 255 : depth + 1));

        return node;
    }

    static Node* Load(std::filesystem::path path)
    {
        std::ifstream is(path, std::ios::in | std::ios::binary);
        return is ? Load(is, nullptr, 0) : nullptr;
    }

    NodeIterator begin()
    {
        return NodeIterator(this, 0);
    }

    NodeIterator end()
    {
        return NodeIterator(this, numChildren);
    }

    std::string ToString()
    {
        if (!parent)
            return name;

        std::string parentStr = parent->ToString();
        if (!parentStr.ends_with('\\'))
            parentStr += '\\';
        parentStr.append(name);

        return std::move(parentStr);
    }

    template<class Fn>
    void ForEach(const Fn& fn)
    {
        fn(*this);
        for (size_t i = 0; i < numChildren; ++i)
            children[i]->ForEach(fn);
    }

    static std::weak_ordering CompareLenLex(const Node& l, const Node& r)
    {
        using order = std::weak_ordering;

        return l.len != r.len
            ? (l.len < r.len ? order::less : order::greater)
            : (l.name <=> r.name);
    }

    static std::weak_ordering CompareDepthLenLex(const Node& l, const Node& r)
    {
        using order = std::weak_ordering;

        if (l.depth != r.depth)
        {
            return l.depth < r.depth ? order::less : order::greater;
        }
        else if (l.name == r.name)
        {
            return order::equivalent;
        }
        else if (l.depth == 0)
        {
            return CompareLenLex(l, r);
        }
        else
        {
            auto o = CompareDepthLenLex(*l.parent, *r.parent);
            return o == order::equivalent ? CompareLenLex(l, r) : o;
        }
    }
};

Node* IndexDrive(char drive);

NodeIndex Flatten(std::vector<NodeView> nodes);