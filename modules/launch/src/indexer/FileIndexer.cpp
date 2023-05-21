#include "FileIndexer.hpp"

#include <unordered_map>
#include <ranges>

NodeIterator::NodeIterator(Node* initial, size_t index)
    : nodes{NodeIteratorRef(initial, index)}
{}

NodeIterator::NodeIterator(const NodeIterator& i)
    : nodes(i.nodes)
{}

Node& NodeIterator::operator*() const noexcept
{
    return *nodes.back().node;
}

Node* NodeIterator::operator->() const noexcept
{
    return nodes.back().node;
}

NodeIterator& NodeIterator::operator++()
{
    NodeIteratorRef* ref = &nodes.back();

    if (ref->index < ref->node->numChildren)
    {
        nodes.emplace_back(ref->node->children[ref->index++], 0);
    }
    else if (nodes.size() > 1)
    {
        do
        {
            nodes.pop_back();
            ref = &nodes.back();
        }
        while (nodes.size() > 1 && ref->index == ref->node->numChildren);

        if (ref->index < ref->node->numChildren)
        {
            nodes.emplace_back(ref->node->children[ref->index++], 0);
        }
    }

    return *this;
}

bool operator==(const NodeIterator &l, const NodeIterator& r)
{
    return l.nodes.back() == r.nodes.back();
}

NodeIndex Flatten(std::vector<NodeView> nodes) {
    std::vector<NodeFlat> flattened;
    std::string str;

    // Pre-assign indexes for each node (and sum str len for next step)
    // This allows for children to be sorted before their parent!
    size_t strLenTotal = 0;
    for (size_t i = 0; i < nodes.size(); ++i)
    {
        nodes[i].node->index = i;
        strLenTotal += nodes[i].node->len;
    }

    // Pre-allocate memory since we know exact sizes!
    flattened.reserve(nodes.size());
    str.reserve(strLenTotal);

    // Copy out path and index data from nodes
    for (auto &view : nodes)
    {
        auto parentIndex = (view.node->parent)
            ? view.node->parent->index
            : view.node->index;

        auto str_offset = str.size();
        str.append(view.node->name);

        flattened.emplace_back(
            (uint32_t)str_offset,
            (uint32_t)parentIndex,
            view.node->depth,
            view.node->len);
    }

    std::cout << "   flattened str = " << str.size() << '\n';
    std::cout << "   flattened nodes = " << nodes.size() << '\n';

    return NodeIndex(std::move(flattened), std::move(str));
}