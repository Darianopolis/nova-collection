#pragma once

#include <nova/core/nova_Core.hpp>

#include "UnicodeCollator.hpp"

using namespace nova::types;

static usz nodesCreated = 0;
static usz nodesDestroyed = 0;

struct Node;

// -----------------------------------------------------------------------------

struct NodeFlat
{
    u32 strOffset;
    u32 parent;
    u8 depth;
    u8 len;
    u8 match;
    u8 inheritedMatch;
};

struct NodeIndex
{
    std::vector<NodeFlat> nodes;
    std::string str;
};

// -----------------------------------------------------------------------------

struct Node
{
    c8* name = nullptr;

    Node* parent = nullptr;
    Node* firstChild = nullptr;
    Node* nextSibling = nullptr;

    u32 index = 0;
    u8 len = 0;
    u8 depth;

    Node(c8* name, u8 len, Node* parent, u8 depth);
    ~Node();

    void AddChild(Node* child);

    usz Count();

    void Save(std::ofstream& os);
    void Save(const std::filesystem::path& path);

    static Node* Load(std::ifstream& is, Node* parent, u8 depth);
    static Node* Load(std::filesystem::path path);

    std::string ToString();

    template<class Fn>
    void ForEach(const Fn& fn)
    {
        fn(*this);
        for (auto node = firstChild; node; node = node->nextSibling)
            node->ForEach(fn);
    }

    static std::weak_ordering CompareLenLex(const Node& l, const Node& r);
    static std::weak_ordering CompareDepthLenLex(const Node& l, const Node& r);
};

NodeIndex Flatten(std::vector<Node*> nodes);

Node* IndexDrive(c8 drive);