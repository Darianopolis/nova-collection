#pragma once

#include "Index.hpp"

#include <sqlite3.h>
#include <PathTree.hpp>
#include <ScopeGuards.hpp>
#include <FileIndexer.hpp>

#include <string>
#include <filesystem>
#include <memory>
#include <ranges>
#include <regex>
#include <execution>

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
public:
    std::vector<ResultList*> lists;

    ResultListPriorityCollector()
    {}

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

        for (int i = 0; i < lists.size(); ++i)
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
        if (!item) {
            // for (auto l : lists.reserve()) {
            for (auto l = lists.rbegin(); l != lists.rend(); l++)
            {
                auto prev = (*l)->Prev(nullptr);
                if (prev)
                    return prev;
            }
        }

        for (int i = 0; i < lists.size(); ++i)
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

static void CheckSql(int rc, sqlite3* db)
{
    if (rc && (rc != SQLITE_DONE && rc != SQLITE_ROW))
        throw std::exception(std::format("SQL error: {}\n", sqlite3_errmsg(db)).c_str());
}

static char* sql_errno;
static void CheckSql(int rc)
{
    if (rc != SQLITE_OK && rc != SQLITE_DONE && rc != SQLITE_ROW)
    {
        sqlite3_free(sql_errno);
        throw std::exception(std::format("SQL error: {}\n", sql_errno).c_str());
    }
}

static int EmptyCallback(void*, int, char**, char**)
{
    return 0;
}

class FavResultItem : public ResultItem
{
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
private:
    std::filesystem::path path;
};

// static const char* appDb = "C:\\Users\\Darian\\.nms\\app.db";
// static char* appDb = boost::filesystem::user;

class FavResultList : public ResultList
{
    std::vector<std::string>* keywords;
    std::vector<std::unique_ptr<FavResultItem>> favourites;
    std::string appDb;

public:
    FavResultList(std::vector<std::string>* _keywords)
        : keywords(_keywords)
        , appDb(getenv("USERPROFILE") + std::string("\\.nms\\app.db"))
    {
        Create();
        Load();
        std::cout << " dir: " << appDb << '\n';
    }

    void Create()
    {
        sqlite3* db{nullptr};
        ON_SCOPE_EXIT(&) {
            sqlite3_close(db);
        };

        CheckSql(sqlite3_open(appDb.c_str(), &db), db);

        CheckSql(sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS \"favourites\"("\
                    "\"path\" TEXT PRIMARY KEY,"\
                    "\"uses\" INTEGER NOT NULL)", EmptyCallback, nullptr, &sql_errno));
    }

    void Load()
    {
        sqlite3* db{nullptr};
        ON_SCOPE_EXIT(&) {
            sqlite3_close(db);
        };

        CheckSql(sqlite3_open(appDb.c_str(), &db), db);

        favourites.clear();
        CheckSql(sqlite3_exec(
                db,
                "SELECT path FROM favourites ORDER BY uses DESC",
                [](void* list, int, char** val, char**) {
                    std::cout << std::format("path = {}\n", val[0]);
                    auto& fav = *static_cast<std::vector<std::unique_ptr<FavResultItem>>*>(list);
                    fav.push_back(std::make_unique<FavResultItem>(val[0]));
                    return 0;
                },
                &favourites,
                &sql_errno));
    }

    void IncrementUses(const std::filesystem::path& path, bool reload = true)
    {
        sqlite3* db{nullptr};
        sqlite3_stmt* stmt{nullptr};
        ON_SCOPE_FAILURE(&) {
            sqlite3_finalize(stmt);
            sqlite3_close(db);
        };

        std::string str = path.string();

        CheckSql(sqlite3_open(appDb.c_str(), &db), db);

        CheckSql(sqlite3_prepare_v2(db, "INSERT OR IGNORE INTO favourites(path, uses) VALUES (?, 0);", -1, &stmt, nullptr), db);
        CheckSql(sqlite3_bind_text(stmt, 1, str.c_str(), (int)str.size(), SQLITE_TRANSIENT), db);
        CheckSql(sqlite3_step(stmt), db);
        CheckSql(sqlite3_finalize(stmt), db);

        CheckSql(sqlite3_prepare_v2(db, "UPDATE favourites SET uses = uses + 1 WHERE path = ?", -1, &stmt, nullptr), db);
        CheckSql(sqlite3_bind_text(stmt, 1, str.c_str(), (int)str.size(), SQLITE_TRANSIENT), db);
        CheckSql(sqlite3_step(stmt), db);

        sqlite3_finalize(stmt);
        sqlite3_close(db);

        if (reload)
            Load();
    }

    void ResetUses(const std::filesystem::path& path, bool reload = true)
    {
        sqlite3* db{nullptr};
        sqlite3_stmt* stmt{nullptr};
        ON_SCOPE_FAILURE(&) {
            sqlite3_finalize(stmt);
            sqlite3_close(db);
        };

        std::string str = path.string();

        CheckSql(sqlite3_open(appDb.c_str(), &db), db);

        CheckSql(sqlite3_prepare_v2(db, "DELETE FROM favourites WHERE path = ?", -1, &stmt, nullptr), db);
        CheckSql(sqlite3_bind_text(stmt, 1, str.c_str(), (int)str.size(), SQLITE_TRANSIENT), db);
        CheckSql(sqlite3_step(stmt), db);

        sqlite3_finalize(stmt);
        sqlite3_close(db);
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
        return std::ranges::find_if(favourites, [&](auto& item) {
            return item->GetPath() == path;
        }) != favourites.end();
    }
};

// -------------------- FILE RESULT ITEM -------------------- //

class FileResultItem : public ResultItem
{
    friend class FileResultList;

    size_t index;
    std::filesystem::path path;
public:
    FileResultItem(std::filesystem::path&& _path, size_t _index)
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
    // std::vector<std::unique_ptr<Node>> roots;
    // std::vector<NodeView> flat;
    NodeIndex index;
    uint8_t matchBits;
    FavResultList* favourites;
    std::vector<std::string> keywords;
    std::unique_ptr<UnicodeCollator> collator;
public:
    FileResultList(FavResultList* _favourites)
        : favourites(_favourites)
        , matchBits(0)
        , collator(UnicodeCollator::NewAsciiCollator())
    {
        using namespace std::chrono;

        auto start = steady_clock::now();
        {
            std::vector<std::unique_ptr<Node>> roots;
            auto* c = Node::Load(getenv("USERPROFILE") + std::string("\\.nms\\C.index"));
            auto* d = Node::Load(getenv("USERPROFILE") + std::string("\\.nms\\D.index"));
            if (c) roots.emplace_back(c);
            if (d) roots.emplace_back(d);
            std::cout << "Loaded nodes in " << duration_cast<milliseconds>(steady_clock::now() - start) << '\n';

            start = steady_clock::now();
            std::vector<NodeView> flat;
            for (auto &root : roots)
                root->ForEach([&](auto& n) { flat.emplace_back(&n); });

            std::sort(std::execution::par_unseq, flat.begin(), flat.end(), [](auto& l, auto& r) {
                return Node::CompareDepthLenLex(*l.node, *r.node) == std::weak_ordering::less;
            });
            std::cout << "Sorted nodes in " << duration_cast<milliseconds>(steady_clock::now() - start) << '\n';

            start = steady_clock::now();
            index = Flatten(flat);
            std::cout << "Flattened nodes in " << duration_cast<milliseconds>(steady_clock::now() - start) << '\n';

            start = steady_clock::now();
        }
        std::cout << "Destroyed temporary nodes in " << duration_cast<milliseconds>(steady_clock::now() - start) << '\n';
    }

    void Filter(uint8_t matchBit, std::string_view keyword, bool lazy)
    {
        std::cout << "refiltering on keyword, [" << keyword << "] lazy = " << lazy << '\n';
        // std::cout << std::format("    match bit = {:08b}\n", matchBit);

        auto needle = std::string{keyword};
        std::transform(needle.begin(), needle.end(), needle.begin(), [](char c) {
            return (char)std::tolower(c);
        });

        using namespace std::chrono;
        auto start = steady_clock::now();
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
        std::cout << "  indexed in " << duration_cast<milliseconds>(steady_clock::now() - start) << '\n';

        auto start2 = steady_clock::now();
        for (auto& n : index.nodes)
        {
            auto& parent = index.nodes[n.parent];
            n.inheritedMatch = (parent.strOffset != n.strOffset)
                ? parent.inheritedMatch | n.match : n.match;
        }
        auto end = steady_clock::now();
        std::cout << "  propogated in " << duration_cast<milliseconds>(end - start2) << '\n';
        std::cout << "  total = " << duration_cast<milliseconds>(end - start) << '\n';

        // size_t total = std::count_if(index.nodes.begin(), index.nodes.end(), [&](auto& n) {
        //   return (n.match & match_bits) == match_bits;
        // });
        // std::cout << "total = " << total << '\n';
    }

    std::string MakeString(uint32_t i)
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
        // std::cout << std::format("before query, match bits = {:08b}\n", match_bits);
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
            auto matchBit = static_cast<uint8_t>(1 << i);
            if (i >= keywords.size())
            {
                // New keyword, update tree match bits
                // std::cout << "Adding new keyword [" << new_keywords[i] << "]\n";
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
            auto matchBit = static_cast<uint8_t>(1 << i);
            // std::cout << "Clearing old keyword [" << keywords[i] << "]\n";
            std::for_each(std::execution::par_unseq, index.nodes.begin(), index.nodes.end(), [&](NodeFlat& p) {
                p.match &= ~matchBit;
                p.inheritedMatch &= ~matchBit;
            });
            matchBits &= ~matchBit;
        }

        keywords = std::move(new_keywords);
        // std::cout << std::format("  After query, match bits = {:08b}\n", match_bits);
    }

    std::unique_ptr<ResultItem> Next(const ResultItem* item) override
    {
        auto* current = dynamic_cast<const FileResultItem*>(item);
        size_t i = current ? current->index + 1 : 0;
        while (i < index.nodes.size())
        {
            auto &node = index.nodes[i];
            if (((node.match | node.inheritedMatch) & matchBits) == matchBits)
            {
                // auto path = std::filesystem::path(flat[i].node->string());
                auto path = std::filesystem::path(MakeString((uint32_t)i));
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
        size_t i = current ? current->index - 1 : index.nodes.size() - 1;
        while (i != -1)
        {
            auto &node = index.nodes[i];
            if (((node.match | node.inheritedMatch) & matchBits) == matchBits)
            {
                auto path = std::filesystem::path(MakeString((uint32_t)i));
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