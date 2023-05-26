#pragma once

#include "Database.hpp"

#include <nova/core/nova_Core.hpp>

#include <FileIndexer.hpp>

using namespace nova::types;

enum class QueryAction
{
    APPEND,
    SHORTEN,
    RESET,
    SET
};

class ResultItem
{
public:
    virtual const std::filesystem::path& GetPath() const = 0;
    virtual ~ResultItem() = default;
};

class ResultList
{
public:
    virtual void Query(QueryAction action, std::string_view query) = 0;
    virtual std::unique_ptr<ResultItem> Next(const ResultItem* item) = 0;
    virtual std::unique_ptr<ResultItem> Prev(const ResultItem* item) = 0;
    virtual bool Contains(const ResultItem& item) = 0;
    virtual bool Filter(const ResultItem& item) = 0;

    virtual ~ResultList() = default;
};


























class ResultListPriorityCollector : public ResultList
{
    std::vector<ResultList*> lists;

public:
    ResultListPriorityCollector() {}

    void AddList(ResultList* list)
    {
        lists.push_back(list);
    }

    void Query(QueryAction action, std::string_view query) override
    {
        for (auto l : lists)
            l->Query(action, query);
    }

    std::unique_ptr<ResultItem> Next(const ResultItem* item)
    {
        if (!item)
        {
            for (auto l : lists)
            {
                auto next = l->Next(nullptr);
                if (next)
                    return next;
            }
        }

        for (i32 i = 0; i < lists.size(); ++i)
        {
            auto l = lists[i];
            if (l->Contains(*item))
            {
                auto next = l->Next(item);
                if (!next)
                {
                    if (i + 1 == lists.size())
                        return nullptr;

                    return lists[i + 1]->Next(nullptr);
                }
                return next;
            }
        }
        return nullptr;
    }

    std::unique_ptr<ResultItem> Prev(const ResultItem* item)
    {
        if (!item)
        {
            for (auto l = lists.rbegin(); l != lists.rend(); l++)
            {
                auto prev = (*l)->Prev(nullptr);
                if (prev)
                    return prev;
            }
        }

        for (i32 i = 0; i < lists.size(); ++i)
        {
            auto l = lists[i];
            if (l->Contains(*item))
            {
                auto prev = l->Prev(item);
                if (!prev)
                {
                    if (i == 0)
                        return nullptr;

                    return lists[i - 1]->Prev(nullptr);
                }
                return prev;
            }
        }
        return nullptr;
    }

    bool Filter(const ResultItem& item)
    {
        for (auto l : lists)
        {
            if (l->Contains(item))
                return l->Filter(item);
        }
        return false;
    }

    bool Contains(const ResultItem& item)
    {
        for (auto l : lists)
        {
            if (l->Contains(item))
                return true;
        }
        return false;
    }
};


























class FavResultItem : public ResultItem
{
    std::filesystem::path path;

public:
    FavResultItem(const std::filesystem::path& _path)
        : path(_path)
    {}

    const std::filesystem::path& GetPath() const override
    {
        return path;
    }

    bool operator==(const FavResultItem& other)
    {
        return GetPath() == other.GetPath();
    }
};

class FavResultList : public ResultList
{
    std::vector<std::string>* keywords;
    std::vector<std::unique_ptr<FavResultItem>> favourites;
    std::string dbName;

public:
    FavResultList(std::vector<std::string>* _keywords)
        : keywords(_keywords)
        , dbName(std::format("{}\\.nms\\app.db", getenv("USERPROFILE")))
    {
        Create();
        Load();
        NOVA_LOG("Database = {}", dbName);
    }

    void Create()
    {
        Database db(dbName);
        Statement(db,
            R"(
                CREATE TABLE IF NOT EXISTS "favourites" (
                    "path" TEXT PRIMARY KEY,
                    "uses" INTEGER NOT NULL
                );
            )")
            .Step();
    }

    void Load()
    {
        Database db(dbName);
        Statement stmt(db, "SELECT path FROM favourites ORDER BY uses DESC");

        favourites.clear();
        while (stmt.Step())
        {
            NOVA_LOG("path = {}", stmt.GetString(1));
            favourites.push_back(std::make_unique<FavResultItem>(stmt.GetString(1)));
        }
    }

    void IncrementUses(const std::filesystem::path& path, bool reload = true)
    {
        std::string str = path.string();

        Database db(dbName);

        Statement(db, "INSERT OR IGNORE INTO favourites(path, uses) VALUES (?, 0)")
            .SetString(1, str)
            .Step();

        Statement(db, "UPDATE favourites SET uses = uses + 1 WHERE path = ?")
            .SetString(1, str)
            .Step();

        if (reload)
            Load();
    }

    void ResetUses(const std::filesystem::path& path, bool reload = true)
    {
        std::string str = path.string();

        Database db(dbName);
        Statement(db, "DELETE FROM favourites WHERE path = ?")
            .SetString(1, str)
            .Step();

        if (reload)
            Load();
    }

    void Query(QueryAction, std::string_view) final {}

    bool Filter(const std::filesystem::path& path)
    {
        std::string str = path.string();
        for (auto& keyword : *keywords)
        {
            if (std::search(
                    str.begin(), str.end(),
                    keyword.begin(), keyword.end(),
                    [](auto c1, auto c2) {
                        return std::toupper(c1) == std::toupper(c2);
                    }) == str.end())
            {
                return false;
            }
        }
        return true;
    }

    std::unique_ptr<ResultItem> Next(const ResultItem* item) final
    {
        auto i = favourites.begin();
        if (item)
        {
            // TODO This should just compare for direct equality
            while (i != favourites.end() && (*i)->GetPath() != item->GetPath())
                i++;

            if (i == favourites.end())
                return nullptr;
            i++;
        }

        while (i != favourites.end() && !Filter((*i)->GetPath()))
            i++;

        return i != favourites.end()
            ? std::make_unique<FavResultItem>((*i)->GetPath())
            : nullptr;
    }

    std::unique_ptr<ResultItem> Prev(const ResultItem* item) final
    {
        auto i = favourites.rbegin();
        if (item)
        {
            // TODO This should just compare for direct equality
            while (i != favourites.rend() && (*i)->GetPath() != item->GetPath())
                i++;

            if (i == favourites.rend())
                return nullptr;

            i++;
        }

        while (i != favourites.rend() && !Filter((*i)->GetPath()))
            i++;

        return i != favourites.rend()
            ? std::make_unique<FavResultItem>((*i)->GetPath())
            : nullptr;
    }

    bool Filter(const ResultItem& item) final
    {
        auto fav = dynamic_cast<const FavResultItem*>(&item);
        return fav && Filter(fav->GetPath());
    }

    bool Contains(const ResultItem& item) final
    {
        return dynamic_cast<const FavResultItem*>(&item) != nullptr;
    }

    bool ContainsPath(const std::filesystem::path& path)
    {
        return std::ranges::find_if(favourites,
            [&](auto& item) {
                return item->GetPath() == path;
            }) != favourites.end();
    }
};


























class FileResultItem : public ResultItem
{
    friend class FileResultList;

    usz index;
    std::filesystem::path path;

public:
    FileResultItem(std::filesystem::path&& _path, usz _index)
        : index(_index)
        , path(_path)
    {}

    virtual const std::filesystem::path& GetPath() const override
    {
        return path;
    }
};

class FileResultList : public ResultList
{
    NodeIndex index;
    u8 matchBits;
    FavResultList* favourites;
    std::vector<std::string> keywords;
    std::unique_ptr<UnicodeCollator> collator;

public:
    FileResultList(FavResultList* _favourites)
        : favourites(_favourites)
        , matchBits(0)
        , collator(UnicodeCollator::NewAsciiCollator())
    {
        NOVA_TIMEIT_RESET();
        {
            std::vector<std::unique_ptr<Node>> roots;
            auto* c = Node::Load(getenv("USERPROFILE") + std::string("\\.nms\\C.index"));
            auto* d = Node::Load(getenv("USERPROFILE") + std::string("\\.nms\\D.index"));
            if (c) roots.emplace_back(c);
            if (d) roots.emplace_back(d);
            NOVA_TIMEIT("loaded-nodes");

            std::vector<Node*> flat;
            for (auto &root : roots)
                root->ForEach([&](auto& n) { flat.emplace_back(&n); });

            std::sort(std::execution::par_unseq, flat.begin(), flat.end(), [](auto& l, auto& r) {
                return Node::CompareDepthLenLex(*l, *r) == std::weak_ordering::less;
            });
            NOVA_TIMEIT("sorted-nodes");

            index = Flatten(flat);
            NOVA_TIMEIT("flatten-nodes");
        }

        mi_collect(true);
        NOVA_TIMEIT("node-cleanup");
    }

    void Filter(u8 matchBit, std::string_view keyword, bool lazy)
    {
        NOVA_LOG("Refiltering on keyword, [{}] lazy = {}", keyword, lazy);

        auto needle = std::string{keyword};
        std::transform(needle.begin(), needle.end(), needle.begin(), [](c8 c) {
            return (c8)std::tolower(c);
        });

        NOVA_TIMEIT_RESET();
        if (lazy)
        {
            std::for_each(std::execution::par_unseq, index.nodes.begin(), index.nodes.end(), [&](auto& view) {
                if ((view.match & matchBit) == matchBit)
                {
                    std::string_view haystack { &index.str[view.strOffset], view.len };
                    if (!collator->FuzzyFind(haystack, needle))
                        view.match &= ~matchBit;
                }
            });
        }
        else
        {
            std::for_each(std::execution::par_unseq, index.nodes.begin(), index.nodes.end(), [&](auto& view) {
                std::string_view haystack { &index.str[view.strOffset], view.len };
                view.match = collator->FuzzyFind(haystack, needle)
                    ? view.match | matchBit
                    : view.match & ~matchBit;
            });
        }
        NOVA_TIMEIT("filter-find");

        for (auto& n : index.nodes)
        {
            auto& parent = index.nodes[n.parent];
            n.inheritedMatch = (parent.strOffset != n.strOffset)
                ? parent.inheritedMatch | n.match : n.match;
        }

        NOVA_TIMEIT("filter-propogated");
    }

    std::string MakeString(u32 i)
    {
        auto &node = index.nodes[i];
        if (node.parent == i)
            return std::string{&index.str[node.strOffset], node.len};
        std::string parent_str = MakeString(node.parent);
        if (!parent_str.ends_with('\\'))
            parent_str += '\\';
        parent_str.append(&index.str[node.strOffset], node.len);
        return std::move(parent_str);
    }

    void Query(QueryAction, std::string_view _query)
    {
        auto query = std::string(_query);
        std::regex words{"\\S+"};
        std::vector<std::string> new_keywords;

        {
            auto i = std::sregex_iterator(query.begin(), query.end(), words);
            auto end = std::sregex_iterator();
            for (; i != end; ++i)
                new_keywords.push_back(i->str());
        }

        for (auto i = 0; i < new_keywords.size(); ++i)
        {
            auto matchBit = static_cast<u8>(1 << i);
            if (i >= keywords.size())
            {
                // New keyword, update tree match bits
                std::for_each(std::execution::par_unseq, index.nodes.begin(), index.nodes.end(), [&](NodeFlat& p) {
                    p.match |= matchBit;
                    p.inheritedMatch &= ~matchBit;
                });
                matchBits |= matchBit;
                Filter(matchBit, new_keywords[i], false);
            }
            else if (keywords[i] != new_keywords[i])
            {
                // Keyword change, refilter match column
                // Memoized - Do a lazy match if new keyword contains the previous key
                Filter(matchBit, new_keywords[i], new_keywords[i].find(keywords[i]) != std::string::npos);
            }
        }

        // Clear any remaining keywords!
        for (auto i = new_keywords.size(); i < keywords.size(); ++i)
        {
            auto matchBit = static_cast<u8>(1 << i);
            std::for_each(std::execution::par_unseq, index.nodes.begin(), index.nodes.end(), [&](NodeFlat& p) {
                p.match &= ~matchBit;
                p.inheritedMatch &= ~matchBit;
            });
            matchBits &= ~matchBit;
        }

        keywords = std::move(new_keywords);
    }

    std::unique_ptr<ResultItem> Next(const ResultItem* item) override
    {
        auto* current = dynamic_cast<const FileResultItem*>(item);
        usz i = current ? current->index + 1 : 0;
        while (i < index.nodes.size())
        {
            auto &node = index.nodes[i];
            if (((node.match | node.inheritedMatch) & matchBits) == matchBits)
            {
                auto path = std::filesystem::path(MakeString((u32)i));
                if (!favourites->ContainsPath(path))
                    return std::make_unique<FileResultItem>(std::move(path), i);
            }
            i++;
        }
        return nullptr;
    }

    std::unique_ptr<ResultItem> Prev(const ResultItem* item) override
    {
        auto* current = dynamic_cast<const FileResultItem*>(item);
        usz i = current ? current->index - 1 : index.nodes.size() - 1;
        while (i != -1)
        {
            auto &node = index.nodes[i];
            if (((node.match | node.inheritedMatch) & matchBits) == matchBits)
            {
                auto path = std::filesystem::path(MakeString((u32)i));
                if (!favourites->ContainsPath(path))
                    return std::make_unique<FileResultItem>(std::move(path), i);
            }
            i--;
        }
        return nullptr;
    };

    bool Contains(const ResultItem& item) override
    {
        return dynamic_cast<const FileResultItem*>(&item);
    }

    bool Filter(const ResultItem& item) override
    {
        auto* current = dynamic_cast<const FileResultItem*>(&item);
        if (!current)
            return false;
        auto& node = index.nodes[current->index];
        return ((node.match | node.inheritedMatch) & matchBits) == matchBits;
    }
};