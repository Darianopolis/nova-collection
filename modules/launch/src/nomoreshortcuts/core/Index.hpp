#pragma once

#include <PathTree.hpp>
#include <UnicodeCollator.hpp>

#include <string>
#include <vector>
#include <regex>
#include <format>

struct Index
{
    // PathTree* tree;
    // UnicodeCollator* collator;

    std::vector<std::string> keywords;

    // Index(PathTree* tree, UnicodeCollator* collator)
    //   : tree(tree)
    //   , collator(collator) {}

    std::filesystem::path path;
    PathTree tree;
    std::unique_ptr<UnicodeCollator> collator;

    Index(const std::filesystem::path& path)
        : path(path)
        , tree(PathTree::Load(getenv("USERPROFILE") + std::string("\\.nms\\tree.bin")))
        , collator(UnicodeCollator::NewAsciiCollator())
    {
        tree.SetMatchBits(1, 1, 0, 0);
        tree.matchBits = 1;
        tree.useInheritMatch = true;
    }

    void Filter(uint8_t matchBit, std::string_view keyword, bool lazy)
    {
        // std::cout << "refiltering on keyword, [" << keyword << "] lazy = " << lazy << '\n';
        // std::cout << std::format("    match bit = {:08b}\n", matchBit);

        auto needle = std::string{keyword};
        std::transform(needle.begin(), needle.end(), needle.begin(), [](char c) {
            return (char)std::tolower(c);
        });

        if (lazy)
        {
            tree.MatchLazy(tree.matchBits, matchBit, [&](std::string_view str) {
                return collator->FuzzyFind(str, needle);
            });
        }
        else
        {
            tree.Match(matchBit, [&](std::string_view str) {
                return collator->FuzzyFind(str, needle);
            });
        }
    }

    void Query(std::string query)
    {
        // std::cout << std::format("before query, match bits = {:08b}\n", tree.matchBits);
        std::regex words{"\\S+"};
        std::vector<std::string> newKeywords;

        {
            auto i = std::sregex_iterator(query.begin(), query.end(), words);
            auto end = std::sregex_iterator();
            for (; i != end; ++i)
                newKeywords.push_back(i->str());
        }

        for (auto i = 0; i < newKeywords.size(); ++i)
        {
            auto matchBit = static_cast<uint8_t>(1 << i);
            if (i >= keywords.size())
            {
                // New keyword, update tree match bits
                tree.SetMatchBits(matchBit, matchBit, matchBit, 0);
                tree.matchBits |= matchBit;
                Filter(matchBit, newKeywords[i], false);
            }
            else if (keywords[i] != newKeywords[i])
            {
                // Keyword change, refilter match column
                // Memoized - Do a lazy match if new keyword contains the previous key
                Filter(matchBit, newKeywords[i], newKeywords[i].find(keywords[i]) != std::string::npos);
            }
        }

        // Clear any remaining keywords!
        for (auto i = newKeywords.size(); i < keywords.size(); ++i)
        {
            // std::cout << "Clearing old keyword [" << keywords[i] << "]\n";
            auto matchBit = static_cast<uint8_t>(1 << i);
            tree.SetMatchBits(matchBit, 0, matchBit, 0);
            tree.matchBits &= ~matchBit;
        }

        keywords = std::move(newKeywords);
        // std::cout << std::format("  After query, match bits = {:08b}\n", tree.matchBits);
    }
};