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

// -----------------------------------------------------------------------------

struct Node
{
    char* name = nullptr;
    Node* parent = nullptr;
    Node* firstChild = nullptr;
    Node* nextSibling = nullptr;
    uint32_t index = 0;
    uint8_t len = 0;
    uint8_t depth;

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
        child->nextSibling = firstChild;
        firstChild = child;
    }

    ~Node()
    {
        nodesDestroyed++;
        if (name)
        {
            delete[] name;
        }

        auto node = firstChild;
        while (node)
        {
            auto next = node->nextSibling;
            delete node;
            node = next;
        }
    }

    size_t Count()
    {
        size_t total = 1;
        for (auto node = firstChild; node; node = node->nextSibling)
            total += node->Count();
        return total;
    }

    void save(std::ofstream& os)
    {
        os.write(reinterpret_cast<const char*>(&len), sizeof(uint8_t));
        os.write(name, len);
        uint32_t numChildren = 0;
        for (auto node = firstChild; node; node = node->nextSibling)
            numChildren++;
        os.write(reinterpret_cast<const char*>(&numChildren), sizeof(uint32_t));

        for (auto node = firstChild; node; node = node->nextSibling)
            node->save(os);
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
        for (auto node = firstChild; node; node = node->nextSibling)
            node->ForEach(fn);
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