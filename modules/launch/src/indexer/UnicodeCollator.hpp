#pragma once

#include <nova/core/nova_Core.hpp>

using namespace nova::types;

class UnicodeCollator
{
public:
    UnicodeCollator(std::vector<c8> utf8Lookup, std::vector<c8> utf32Lookup);

    inline bool Compare(std::string_view value, usz& index, const c8 c) const;
    bool FuzzyFind(std::string_view value, std::string_view str) const;
    std::string ConvertToPlainAscii(std::string_view value) const;

    static std::unique_ptr<UnicodeCollator> NewAsciiCollator();

private:
    std::vector<c8> utf8Lookup;
    std::vector<c8> utf32Lookup;
};
