#pragma once

#include <nova/core/nova_Core.hpp>

#include <string>
#include <string_view>
#include <cctype>
#include <iostream>
#include <vector>
#include <memory>

using namespace nova::types;

class UnicodeCollator
{
public:
    UnicodeCollator(const std::vector<c8> utf8Lookup, const std::vector<c8> utf32Lookup);

    inline bool Compare(const std::string_view& value, usz& index, const c8 c) const;
    bool FuzzyFind(const std::string_view& value, const std::string& str) const;
    std::string ConvertToPlainAscii(const std::string& value) const;

    static std::unique_ptr<UnicodeCollator> NewAsciiCollator();

private:
    std::vector<c8> utf8Lookup;
    std::vector<c8> utf32Lookup;
};
