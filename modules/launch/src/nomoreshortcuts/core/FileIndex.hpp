#pragma once

#include <Platform.hpp>

#include <vector>

namespace nms
{
    struct FileNode
    {
        std::string name;

        u32 depth = 0;
        u32 uses = 0;

        bool filtered = false;

        FileNode* parent = nullptr;
        FileNode* firstChild = nullptr;
        FileNode* nextSibling = nullptr;

    public:
        ~FileNode();

        void AddChild(FileNode* child);
        u32 NumChildren();

        std::string ToString();

        template<class Fn>
        void ForEach(const Fn& fn)
        {
            fn(*this);
            for (auto node = firstChild; node; node = node->nextSibling)
                node->ForEach(fn);
        }

        void Save(std::ofstream& os);
        void Load(std::ifstream& is, u32 depth);

        static std::weak_ordering CompareLenLex(const FileNode& l, const FileNode& r);
        static std::weak_ordering CompareDepthLenLex(const FileNode& l, const FileNode& r);
    };

    struct FileNodeRef
    {
        FileNode* node;
        u32 flatIndex;
    };

    struct FileIndex
    {
        FileNode* root = nullptr;

        std::vector<FileNode*> nodes;

        struct Keyword
        {
            std::string keyword;
            u32 bitMask;
        };
        std::vector<Keyword> keywords;

    public:
        void Save(const std::filesystem::path& path);
        void Load(const std::filesystem::path& path);

        void Index();

        void Flatten();
        void Query(nova::Span<std::string_view> keywords);
        bool IsEmpty();
        std::optional<FileNodeRef> First();
        std::optional<FileNodeRef> Last();
        std::optional<FileNodeRef> Next(FileNodeRef node);
        std::optional<FileNodeRef> Prev(FileNodeRef node);
    };
}