#include "UnicodeCollator.hpp"

UnicodeCollator::UnicodeCollator(
        std::vector<char> _utf8Lookup,
        std::vector<char> _utf32Lookup)
    : utf8Lookup(std::move(_utf8Lookup))
    , utf32Lookup(std::move(_utf32Lookup))
{}

std::unique_ptr<UnicodeCollator> UnicodeCollator::NewAsciiCollator()
{
    std::vector<char> utf8Lookup(65'565);
    std::vector<char> utf32Lookup(8'388'608);

    for (int a = 0; a < 128; ++a)
    {
        utf8Lookup[a] = (char)std::tolower(a);
        for (int b = 0; b < 256; ++b)
            utf8Lookup[(static_cast<char16_t>(a) << 8) + b] = (char)std::tolower(a);
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
        const char to = (char)std::tolower(l[0]);
        for (int i = 1; i < l.length(); i += 25)
        {
            const int codepoint = ((unsigned char)l[i] << 8) + (unsigned char)l[i + 1];
            utf8Lookup[codepoint] = to;

            const int code32 = ((unsigned char)l[i] & 0b0001'1111)
                | (((unsigned char)l[i + 1] & 0b0011'1111) << 5);

            utf32Lookup[code32] = to;
        }
    }

    return std::make_unique<UnicodeCollator>(std::move(utf8Lookup), std::move(utf32Lookup));
}

std::string UnicodeCollator::ConvertToPlainAscii(const std::string& value) const
{
    std::string out;
    size_t len = value.length();
    size_t i = 0;
    while (i < len)
    {
        unsigned char c = (unsigned char)value[i];
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

bool UnicodeCollator::Compare(const std::string_view& value, size_t& index, const char c) const
{
    const unsigned char first = value[index];
    char n = utf8Lookup[first];
    if (first > 127)
    {
        if (first < 224)
        {
            n = utf32Lookup[(first & 0b0001'1111) + (((unsigned char)(value[++index])) << 5)];
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

bool UnicodeCollator::FuzzyFind(const std::string_view& value, const std::string& str) const
{
    const size_t valueCount = value.length();
    const size_t strCount = str.length();

    if (strCount > valueCount)
        return false;
    if (strCount == 0)
        return true;

    const char first = str[0];
    const size_t max = valueCount - strCount;

    for (size_t i = 0; i <= max; ++i)
    {
        if (!this->Compare(value, i, first))
        {
            while (++i <= max && !this->Compare(value, i, first));
        }

        if (i <= max)
        {
            size_t j = i + 1;
            const size_t true_end = j + strCount - 1;
            const size_t end = (valueCount > true_end) ? true_end : valueCount;
            for (size_t k = 1
                ; j < end && this->Compare(value, j, str[k])
                ; ++j, ++k);

            if (j == true_end)
                return true;
        }
    }

    return false;
}