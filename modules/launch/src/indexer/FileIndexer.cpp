#include "FileIndexer.hpp"

#include <nova/core/nova_Core.hpp>

#include <Platform.hpp>

// -----------------------------------------------------------------------------

Node::Node(c8* _name, u8 _len, Node* _parent, u8 _depth)
    : name(_name)
    , len(_len)
    , parent(_parent)
    , depth(_depth)
{
    nodesCreated++;
}

Node::~Node()
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

void Node::AddChild(Node* child)
{
    child->parent = this;
    child->nextSibling = firstChild;
    firstChild = child;
}

usz Node::Count()
{
    usz total = 1;
    for (auto node = firstChild; node; node = node->nextSibling)
        total += node->Count();
    return total;
}

void Node::Save(std::ofstream& os)
{
    os.write(reinterpret_cast<const c8*>(&len), sizeof(u8));
    os.write(name, len);
    u32 numChildren = 0;
    for (auto node = firstChild; node; node = node->nextSibling)
        numChildren++;
    os.write(reinterpret_cast<const c8*>(&numChildren), sizeof(u32));

    for (auto node = firstChild; node; node = node->nextSibling)
        node->Save(os);
}

void Node::Save(const std::filesystem::path& path)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream os(path, std::ios::out | std::ios::binary);

    if (os)
    {
        Save(os);
        os.close();
    }
}

Node* Node::Load(std::ifstream& is, Node* parent, u8 depth)
{
    u8 len;
    is.read(reinterpret_cast<c8*>(&len), 1);

    c8* chars = new c8[len + 1];
    is.read(chars, len);
    chars[len] = '\0';

    u32 numChildren;
    is.read(reinterpret_cast<c8*>(&numChildren), 4);

    Node* node = new Node(chars, len, parent, depth);
    for (usz i = 0; i < numChildren; ++i)
        node->AddChild(Load(is, node, depth == 255 ? 255 : depth + 1));

    return node;
}

Node* Node::Load(std::filesystem::path path)
{
    std::ifstream is(path, std::ios::in | std::ios::binary);
    return is ? Load(is, nullptr, 0) : nullptr;
}

std::string Node::ToString()
{
    if (!parent)
        return name;

    std::string parentStr = parent->ToString();
    if (!parentStr.ends_with('\\'))
        parentStr += '\\';
    parentStr.append(name);

    return std::move(parentStr);
}

std::weak_ordering Node::CompareLenLex(const Node& l, const Node& r)
{
    using order = std::weak_ordering;

    return l.len != r.len
        ? (l.len < r.len ? order::less : order::greater)
        : (l.name <=> r.name);
}

std::weak_ordering Node::CompareDepthLenLex(const Node& l, const Node& r)
{
    using order = std::weak_ordering;

    if (l.depth != r.depth)
    {
        return l.depth < r.depth ? order::less : order::greater;
    }
    else
    if (l.name == r.name)
    {
        return order::equivalent;
    }
    else
    if (l.depth == 0)
    {
        return CompareLenLex(l, r);
    }
    else
    {
        auto o = CompareDepthLenLex(*l.parent, *r.parent);
        return o == order::equivalent ? CompareLenLex(l, r) : o;
    }
}

// -----------------------------------------------------------------------------
//                                  Flatten
// -----------------------------------------------------------------------------

NodeIndex Flatten(std::vector<Node*> nodes) {
    std::vector<NodeFlat> flattened;
    std::string str;

    // Pre-assign indexes for each node (and sum str len for next step)
    // This allows for children to be sorted before their parent!
    usz strLenTotal = 0;
    for (u32 i = 0; i < nodes.size(); ++i)
    {
        nodes[i]->index = i;
        strLenTotal += nodes[i]->len;
    }

    // Pre-allocate memory since we know exact sizes!
    flattened.reserve(nodes.size());
    str.reserve(strLenTotal);

    // Copy out path and index data from nodes
    for (auto* view : nodes)
    {
        auto parentIndex = (view->parent)
            ? view->parent->index
            : view->index;

        auto strOffset = str.size();
        str.append(view->name);

        flattened.emplace_back(
            (u32)strOffset,
            (u32)parentIndex,
            view->depth,
            view->len);
    }

    NOVA_LOG("   flattened str = {}", str.size());
    NOVA_LOG("   flattened nodes = {}", nodes.size());

    return NodeIndex(std::move(flattened), std::move(str));
}

// -----------------------------------------------------------------------------
//                             Windows indexing
// -----------------------------------------------------------------------------

struct WinIndexer
{
    c16 path[32767];
    c8 utf8Buffer[1024];
    WIN32_FIND_DATA result;
    usz count = 0;

    WinIndexer(const c16* root)
    {
        wcscpy(path, root);
    }

    static void Index(Node* node, const c16* root)
    {
        WinIndexer s(root);
        NOVA_LOG("Indexing {}", nms::ConvertToString(root));
        s.Search(node, wcslen(root), 0);
    }

    void Search(Node* node, usz offset, u8 depth)
    {
        path[offset] = '\\';
        path[offset + 1] = '*';
        path[offset + 2] = '\0';

        HANDLE findHandle = FindFirstFileEx(
            path,
            FindExInfoBasic,
            &result,
            FindExSearchNameMatch,
            nullptr,
            FIND_FIRST_EX_LARGE_FETCH);

        if (findHandle == INVALID_HANDLE_VALUE)
            return;

        do
        {
            usz len = wcslen(result.cFileName);

            // Ignore current (.) and parent (..) directories!
            if (len == 0 || (result.cFileName[0] == '.' && (len == 1 || (len == 2 && result.cFileName[1] == '.'))))
                continue;

            BOOL usedDefault = FALSE;
            usz utf8Len = WideCharToMultiByte(
                CP_UTF8,
                0,
                result.cFileName,
                (i32)len,
                utf8Buffer,
                (i32)sizeof(utf8Buffer) - 1,
                nullptr,
                &usedDefault);

            if (utf8Len == 0)
            {
                NOVA_LOG("Failed to convert {}", nms::ConvertToString(result.cFileName));
                continue;
            }

            c8* name = new c8[utf8Len + 1];
            memcpy(name, utf8Buffer, utf8Len);
            name[utf8Len] = '\0';
            Node* child = new Node(name, (u8)utf8Len, node, depth);
            node->AddChild(child);

            if (++count % 10000 == 0)
            {
                NOVA_LOG("Files = {}, path = {}", count, nms::ConvertToString(path));
            }

            if (result.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                memcpy(&path[offset + 1], result.cFileName, 2 * len);
                Search(child, offset + len + 1, depth == 255 ? 255 : depth + 1);
            }
        }
        while (FindNextFile(findHandle, &result));

        FindClose(findHandle);
    }
};

Node* IndexDrive(c8 driverLetter)
{
    NOVA_LOG("Indexing drive: {}", driverLetter);
    c8 upper = (c8)std::toupper(driverLetter);

    NOVA_LOG("Drive letter = {}", upper);

    auto* node = new Node(new c8[] { upper, ':', '\\', '\0' }, 3, nullptr, 0);
    c16 init[] { L'\\', L'\\', L'?', L'\\', c16(upper), L':', };
    WinIndexer::Index(node, init);

    NOVA_LOG("Indexed!");

    return node;
}