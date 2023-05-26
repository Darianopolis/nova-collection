#include "UnicodeCollator.hpp"

UnicodeCollator::UnicodeCollator(
        std::vector<c8> _utf8Lookup,
        std::vector<c8> _utf32Lookup)
    : utf8Lookup(std::move(_utf8Lookup))
    , utf32Lookup(std::move(_utf32Lookup))
{}

std::unique_ptr<UnicodeCollator> UnicodeCollator::NewAsciiCollator()
{
    std::vector<c8> utf8Lookup(65'565);
    std::vector<c8> utf32Lookup(8'388'608);

    for (i32 a = 0; a < 128; ++a)
    {
        utf8Lookup[a] = (c8)std::tolower(a);
        for (i32 b = 0; b < 256; ++b)
            utf8Lookup[(static_cast<char16_t>(a) << 8) + b] = (c8)std::tolower(a);
    }

    const std::string changers[] {
        "aàáảãạăằắẳẵặâầấẩẫậ",
        "AÀÁẢÃẠĂẰẮẲẴẶÂẦẤẨẪẬ",
        "OÒÒÓỎÕỌÔỒỐỔỖỘƠỜỚỞỠỢ",
        "EÈÉẺẼẸÊỀẾỂỄỆ",
        "UÙÚỦŨỤƯỪỨỬỮỰ",
        "IÌÍỈĨỊ",
        "YỲÝỶỸỴ",
        "DĐ",
        "oòóỏõọôồốổỗộơờớởỡợ",
        "eèéẻẽẹêềếểễệ",
        "uùúủũụưừứửữự",
        "iìíỉĩị",
        "yỳýỷỹỵ",
        "dđ"
    };

    for (const std::string& l : changers)
    {
        const c8 to = (c8)std::tolower(l[0]);
        for (i32 i = 1; i < l.length(); i += 25)
        {
            const i32 codepoint = ((uc8)l[i] << 8) + (uc8)l[i + 1];
            utf8Lookup[codepoint] = to;

            const i32 code32 = ((uc8)l[i] & 0b0001'1111)
                | (((uc8)l[i + 1] & 0b0011'1111) << 5);

            utf32Lookup[code32] = to;
        }
    }

    return std::make_unique<UnicodeCollator>(std::move(utf8Lookup), std::move(utf32Lookup));
}

bool UnicodeCollator::Compare(std::string_view value, usz& index, const c8 c) const
{
    const uc8 first = value[index];
    c8 n = utf8Lookup[first];
    if (first > 127)
    {
        if (first < 224)
        {
            n = utf32Lookup[(first & 0b0001'1111) + (((uc8)(value[++index])) << 5)];
            if (n == 0)
                n = '?';
        }
        else
        {
            index += (first < 240) ? 3 : 2;
            n = '?';
        }
    }

    return n == c;
}

bool UnicodeCollator::FuzzyFind(std::string_view value, std::string_view str) const
{
    const usz valueCount = value.length();
    const usz strCount = str.length();

    if (strCount > valueCount)
        return false;
    if (strCount == 0)
        return true;

    const c8 first = str[0];
    const usz max = valueCount - strCount;

    for (usz i = 0; i <= max; ++i)
    {
        if (!this->Compare(value, i, first))
        {
            while (++i <= max && !this->Compare(value, i, first));
        }

        if (i <= max)
        {
            usz j = i + 1;
            const usz true_end = j + strCount - 1;
            const usz end = (valueCount > true_end) ? true_end : valueCount;
            for (usz k = 1
                ; j < end && this->Compare(value, j, str[k])
                ; ++j, ++k);

            if (j == true_end)
                return true;
        }
    }

    return false;
}

std::string UnicodeCollator::ConvertToPlainAscii(std::string_view value) const
{
    std::string out;
    usz len = value.length();
    usz i = 0;
    while (i < len)
    {
        uc8 c = (uc8)value[i];
        if (c > 127)
        {
            out.push_back('?');
            if (c < 224) i += 2;
            else if (c < 240) i += 3;
            else i += 4;
        }
        else
        {
            out.push_back(c);
            ++i;
        }
    }
    return out;
}