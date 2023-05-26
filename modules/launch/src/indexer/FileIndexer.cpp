#include "FileIndexer.hpp"

#include <unordered_map>
#include <ranges>

NodeIndex Flatten(std::vector<NodeView> nodes) {
    std::vector<NodeFlat> flattened;
    std::string str;

    // Pre-assign indexes for each node (and sum str len for next step)
    // This allows for children to be sorted before their parent!
    size_t strLenTotal = 0;
    for (uint32_t i = 0; i < nodes.size(); ++i)
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

        auto strOffset = str.size();
        str.append(view.node->name);

        flattened.emplace_back(
            (uint32_t)strOffset,
            (uint32_t)parentIndex,
            view.node->depth,
            view.node->len);
    }

    NOVA_LOG("   flattened str = {}", str.size());
    NOVA_LOG("   flattened nodes = {}", nodes.size());

    return NodeIndex(std::move(flattened), std::move(str));
}