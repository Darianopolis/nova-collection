#pragma once

#include <nova/core/nova_Core.hpp>
#include <nova/db/nova_Sqlite.hpp>

#include <file_searcher.hpp>

using namespace nova::types;

class ResultItem
{
public:
    virtual const std::filesystem::path& GetPath() const = 0;
    virtual ~ResultItem() = default;
};

class ResultList
{
public:
    void Filter(nova::Span<std::string> query)
    {
        auto view = NOVA_ALLOC_STACK(std::string_view, query.size());
        u32 count = 0;
        for (u32 i = 0; i < query.size(); ++i) {
            if (query[i].size()) {
                view[count++] = query[i];
            }
        }
        Filter(nova::Span(view, count));
    }
    virtual void Filter(nova::Span<std::string_view> query) = 0;
    virtual std::unique_ptr<ResultItem> Next(const ResultItem* item) = 0;
    virtual std::unique_ptr<ResultItem> Prev(const ResultItem* item) = 0;
    virtual bool Contains(const ResultItem& item) = 0;
    virtual bool Filter(const ResultItem& item) = 0;

    virtual ~ResultList() = default;
};

// -----------------------------------------------------------------------------

class ResultListPriorityCollector : public ResultList
{
    std::vector<ResultList*> lists;

public:
    using ResultList::Filter;

    ResultListPriorityCollector() {}

    void AddList(ResultList* list)
    {
        lists.push_back(list);
    }

    void Filter(nova::Span<std::string_view> query)
    {
        for (auto l : lists)
            l->Filter(query);
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

// -----------------------------------------------------------------------------

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
    std::vector<std::string> keywords;
    std::vector<std::unique_ptr<FavResultItem>> favourites;
    std::string dbName;

public:
    using ResultList::Filter;

    FavResultList()
        : dbName(std::format("{}\\.nms\\app.db", getenv("USERPROFILE")))
    {
        Create();
        Load();
        NOVA_LOG("Database = {}", dbName);
    }

    void Create()
    {
        nova::Database db(dbName);
        nova::Statement(db,
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
        nova::Database db(dbName);
        nova::Statement stmt(db, "SELECT path FROM favourites ORDER BY uses DESC");

        favourites.clear();
        while (stmt.Step())
            favourites.push_back(std::make_unique<FavResultItem>(stmt.GetString(1)));
    }

    void IncrementUses(const std::filesystem::path& path, bool reload = true)
    {
        std::string str = path.string();

        nova::Database db(dbName);

        nova::Statement(db, "INSERT OR IGNORE INTO favourites(path, uses) VALUES (?, 0)")
            .SetString(1, str)
            .Step();

        nova::Statement(db, "UPDATE favourites SET uses = uses + 1 WHERE path = ?")
            .SetString(1, str)
            .Step();

        if (reload)
            Load();
    }

    void ResetUses(const std::filesystem::path& path, bool reload = true)
    {
        std::string str = path.string();

        nova::Database db(dbName);
        nova::Statement(db, "DELETE FROM favourites WHERE path = ?")
            .SetString(1, str)
            .Step();

        if (reload)
            Load();
    }

    void Filter(nova::Span<std::string_view> query) final
    {
        keywords.assign(query.begin(), query.end());
    }

    bool Filter(const std::filesystem::path& path)
    {
        std::string str = path.string();
        for (auto& keyword : keywords)
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

// -----------------------------------------------------------------------------

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
    file_searcher_t* searcher;
    FavResultList* favourites;

public:
    using ResultList::Filter;

    FileResultList(file_searcher_t* _searcher, FavResultList* _favourites)
        : searcher(_searcher)
        , favourites(_favourites)
    {}

    void Filter(nova::Span<std::string_view> query)
    {
        NOVA_LOGEXPR(searcher);
        searcher->filter(query);
    }

    std::unique_ptr<ResultItem> Next(const ResultItem* item) override
    {
        auto* current = dynamic_cast<const FileResultItem*>(item);
        uint32_t i = current ? uint32_t(current->index) : UINT_MAX;
        while ((i = searcher->find_next_file(i)) != UINT_MAX) {
            auto path = std::filesystem::path(searcher->index->get_full_path(i));
            if (!favourites->ContainsPath(path)) {
                return std::make_unique<FileResultItem>(std::move(path), i);
            }
        }
        return nullptr;
    }

    std::unique_ptr<ResultItem> Prev(const ResultItem* item) override
    {
        auto* current = dynamic_cast<const FileResultItem*>(item);
        uint32_t i = current ? uint32_t(current->index) : UINT_MAX;
        while ((i = searcher->find_prev_file(i)) != UINT_MAX) {
            auto path = std::filesystem::path(searcher->index->get_full_path(i));
            if (!favourites->ContainsPath(path)) {
                return std::make_unique<FileResultItem>(std::move(path), i);
            }
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
        return current ? searcher->is_matched(uint32_t(current->index)) : false;
    }
};