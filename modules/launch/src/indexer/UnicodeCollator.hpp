#pragma once

#include <string>
#include <string_view>
#include <cctype>
#include <iostream>
#include <vector>
#include <memory>

class UnicodeCollator
{
public:
    inline bool Compare(const std::string_view& value, size_t& index, const char c) const;
    bool FuzzyFind(const std::string_view& value, const std::string& str) const;
    std::string ConvertToPlainAscii(const std::string& value) const;

    static std::unique_ptr<UnicodeCollator> NewAsciiCollator();

private:
    std::vector<char> utf8Lookup;
    std::vector<char> utf32Lookup;

    UnicodeCollator(const std::vector<char> utf8_lookup, const std::vector<char> utf32_lookup);
};