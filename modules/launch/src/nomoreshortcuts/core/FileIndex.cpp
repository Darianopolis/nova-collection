#include "FileIndex.hpp"

namespace nms
{
    FileNode::~FileNode()
    {
        auto node = firstChild;
        while (node)
        {
            auto next = node->nextSibling;
            delete node;
            node = next;
        }
    }

    void FileNode::AddChild(FileNode* child)
    {
        child->parent = this;
        child->nextSibling = firstChild;
        firstChild = child;
    }

u32 FileNode::NumChildren()
{
    u32 total = 0;
    for (auto node = firstChild; node; node = node->nextSibling)
        total++;
    return total;
}

    std::string FileNode::ToString()
    {
        if (!parent)
            return name;

        auto parentStr = parent->ToString();
        if (!parentStr.empty() && !parentStr.ends_with('\\'))
            parentStr += '\\';
        parentStr.append(name);

        return std::move(parentStr);
    }

    void FileNode::Save(std::ofstream& os)
    {
        // NOVA_LOG("Saving \"{}\", depth = {}", ToString(), depth);

        u8 length = u8(name.size());
        os.write((const char*)&length, sizeof(length));
        os.write(name.data(), length);

        u32 numChildren = NumChildren();
        // NOVA_LOG("  numChildren = {}", numChildren);
        os.write((const char*)&numChildren, sizeof(numChildren));

        for (auto* node = firstChild; node; node = node->nextSibling)
            node->Save(os);
    }

    void FileNode::Load(std::ifstream& is, u32 _depth)
    {
        depth = _depth;

        u8 length;
        is.read((char*)&length, sizeof(length));
        name.resize(length);
        is.read(name.data(), length);

        // NOVA_LOG("Loading \"{}\", depth = {}", ToString(), depth);

        u32 numChildren;
        is.read((char*)&numChildren, sizeof(numChildren));
        // NOVA_LOG("  numChildren = {}", numChildren);

        for (u32 i = 0; i < numChildren; ++i)
        {
            auto* node = new FileNode;
            AddChild(node);
            node->Load(is, depth + 1);
        }
    }

    std::weak_ordering FileNode::CompareLenLex(const FileNode& l, const FileNode& r)
    {
        using order = std::weak_ordering;

        if (l.uses != r.uses)
            return l.uses > r.depth ? order::less : order::greater;

        return l.name.size() != r.name.size()
            ? (l.name.size() < r.name.size() ? order::less : order::greater)
            : (l.name <=> r.name);
    }

    std::weak_ordering FileNode::CompareDepthLenLex(const FileNode& l, const FileNode& r)
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

    void FileIndex::Save(const std::filesystem::path& path)
    {
        std::ofstream os(path, std::ios::out | std::ios::binary);

        root->Save(os);
    }

    void FileIndex::Load(const std::filesystem::path& path)
    {
        std::ifstream is(path, std::ios::in | std::ios::binary);

        delete root;
        root = new FileNode;
        root->Load(is, 0);
    }

// -----------------------------------------------------------------------------

    void FileIndex::Flatten()
    {
        nodes.clear();
        root->ForEach([&](auto& node) {
            nodes.emplace_back(&node);
        });

        std::sort(
            std::execution::par_unseq,
            nodes.begin(), nodes.end(),
                [](auto& l, auto& r) {
                return FileNode::CompareDepthLenLex(*l, *r) == std::weak_ordering::less;
            });
    }

    void FileIndex::Query(nova::Span<std::string_view> keywords)
    {
        for (auto* node : nodes)
            node->filtered = true;

        for (auto keyword : keywords)
        {
#pragma omp parallel for
            for (u32 i = 0; i < nodes.size(); ++i)
            {
                auto* node = nodes[i];

                if (node->filtered)
                {
                    if (!node->name.contains(keyword))
                        node->filtered = false;
                }
            }
        }
    }

    bool FileIndex::IsEmpty()
    {
        return nodes.empty();
    }

    std::optional<FileNodeRef> FileIndex::First()
    {
        if (IsEmpty())
            return std::nullopt;

        return FileNodeRef { nodes.front(), 0u };
    }

    std::optional<FileNodeRef> FileIndex::Last()
    {
        if (IsEmpty())
            return std::nullopt;

        return FileNodeRef { nodes.back(), u32(nodes.size()) - 1 };
    }

    std::optional<FileNodeRef> FileIndex::Next(FileNodeRef node)
    {
        u32 index = node.flatIndex + 1;
        while (index < nodes.size())
        {
            if (nodes[index]->filtered)
                return FileNodeRef { nodes[index], index };

            index++;
        }

        return node.flatIndex >= nodes.size() ? node : Last();
    }

    std::optional<FileNodeRef> FileIndex::Prev(FileNodeRef node)
    {
        u32 index = node.flatIndex;
        while (index > 0)
        {
            index--;

            if (nodes[index]->filtered)
                return FileNodeRef { nodes[index], index };
        }

        return node.flatIndex >= nodes.size() ? node : Last();
    }

// -----------------------------------------------------------------------------

    struct Indexer
    {
        c16 path[32'767];
        c8 buffer[32'767];
        WIN32_FIND_DATA result;
        usz count = 0;

        static void Index(FileNode* root)
        {
            c16 driveNames[1024];
            GetLogicalDriveStringsW(1023, driveNames);

            std::vector<c16*> drives;
            for (c16* drive = driveNames; *drive; drive += wcslen(drive) + 1)
                drives.push_back(drive);

            for (u32 i = 0; i < drives.size(); ++i)
            {
                auto drive = drives[i];

                std::wcout << std::format(L"Drive [{}]\n", drive);

                // Remove trailing '\' and convert to uppercase

                drive[wcslen(drive) - 1] = L'\0';
                drive = _wcsupr(drive);

                Indexer indexer;
                indexer.path[0] = L'\\';
                indexer.path[1] = L'\\';
                indexer.path[2] = L'?';
                indexer.path[3] = L'\\';

                wcscpy(indexer.path + 4, drive);

                std::wcout << std::format(L"  searching [{}]\n", indexer.path);

                auto node = new FileNode { .name = nms::ConvertToString(drive), };
                root->AddChild(node);
                indexer.Search(wcslen(indexer.path), 0, node);
            }
        }

        void Search(usz offset, u32 depth, FileNode* parent)
        {
            path[offset + 0] = L'\\';
            path[offset + 1] = L'*';
            path[offset + 2] = L'\0';

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

                // Ignore empty, current "." and parent ".." directories
                if (len == 0 || (result.cFileName[0] == '.' && (len == 1 || (len == 2) && result.cFileName[1] == '.')))
                    continue;

                BOOL usedDefault = FALSE;
                size_t utf8Len = WideCharToMultiByte(
                    CP_UTF8,
                    0,
                    result.cFileName,
                    (int)len,
                    buffer,
                    (int)sizeof(buffer) - 1,
                    nullptr,
                    &usedDefault);
                buffer[utf8Len] = '\0';

                if (utf8Len == 0)
                {
                    std::wcout << std::format(L"Failed to convert {}\n", result.cFileName);
                    continue;
                }

                if (++count % 10'000 == 0)
                {
                    std::wcout << std::format(L"Files = {}, path = {:.{}s}\\{}\n", count, path, offset, result.cFileName);
                }

                auto* node = new FileNode { .name = buffer, .depth = depth + 1 };
                parent->AddChild(node);

                if (result.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                {
                    std::memcpy(&path[offset + 1], result.cFileName, 2 * len);
                    Search(offset + len + 1, depth + 1, node);
                }
            }
            while (FindNextFile(findHandle, &result));

            FindClose(findHandle);
        }
    };

    void FileIndex::Index()
    {
        delete root;

        root = new FileNode;
        Indexer::Index(root);

        // auto c = new FileNode;
        // auto d = new FileNode;
        // auto users = new FileNode;

        // root->AddChild(c);
        // root->AddChild(d);
        // c->AddChild(users);

        // c->name = "C:\\";
        // d->name = "D:\\";
        // users->name = "Users";
    }
}