#include "fs_index.hpp"

#include <unordered_map>
#include <ranges>

// NodeView::NodeView(Node* node)
//   : node(node)
//   , name(node->name)
//   , len(node->len) { }

NodeIterator::NodeIterator(Node* initial, size_t index)
    : nodes{NodeIteratorRef(initial, index)}
{}

NodeIterator::NodeIterator(const NodeIterator& i)
    : nodes(i.nodes)
{}

Node& NodeIterator::operator *() const noexcept
{
    return *nodes.back().node;
}

Node* NodeIterator::operator ->() const noexcept
{
    return nodes.back().node;
}

NodeIterator& NodeIterator::operator ++()
{
    NodeIteratorRef *ref = &nodes.back();

    if (ref->index < ref->node->n_children)
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
        while (nodes.size() > 1 && ref->index == ref->node->n_children);

        if (ref->index < ref->node->n_children)
        {
            nodes.emplace_back(ref->node->children[ref->index++], 0);
        }
    }

    return *this;
}

bool operator ==(const NodeIterator &l, const NodeIterator& r)
{
    return l.nodes.back() == r.nodes.back();
}

NodeIndex flatten(std::vector<NodeView> nodes) {
    std::vector<NodeFlat> flattened;
    std::string str;

    // Pre-assign indexes for each node (and sum str len for next step)
    // This allows for children to be sorted before their parent!
    size_t str_len_total = 0;
    for (size_t i = 0; i < nodes.size(); ++i)
    {
        nodes[i].node->index = i;
        str_len_total += nodes[i].node->len;
    }

    // Pre-allocate memory since we know exact sizes!
    flattened.reserve(nodes.size());
    str.reserve(str_len_total);

    // Copy out path and index data from nodes
    for (auto &view : nodes)
    {
        auto parent_index = (view.node->parent)
            ? view.node->parent->index
            : view.node->index;

        auto str_offset = str.size();
        str.append(view.node->name);

        flattened.emplace_back(
            (uint32_t)str_offset,
            (uint32_t)parent_index,
            view.node->depth,
            view.node->len);
    }

    std::cout << "   flattened str = " << str.size() << '\n';
    std::cout << "   flattened nodes = " << nodes.size() << '\n';

    return NodeIndex(std::move(flattened), std::move(str));
}