#include "UnicodeCollator.hpp"

UnicodeCollator::UnicodeCollator(
  const std::vector<char> utf8_lookup,
  const std::vector<char> utf32_lookup)
  : utf8_lookup(utf8_lookup)
  , utf32_lookup(utf32_lookup) {
}

std::unique_ptr<UnicodeCollator> UnicodeCollator::new_ascii_collator()
{
  std::vector<char> utf8_lookup(65'565);
  std::vector<char> utf32_lookup(8'388'608);

  for (int a = 0; a < 128; ++a) {
    utf8_lookup[a] = std::tolower(a);
    for (int b = 0; b < 256; ++b) {
      utf8_lookup[(static_cast<char16_t>(a) << 8) + b] = std::tolower(a);
    }
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

  for (const std::string& l : changers) {
    const char to = std::tolower(l[0]);
    for (int i = 1; i < l.length(); i += 25) {
      const int codepoint = ((unsigned char)l[i] << 8) + (unsigned char)l[i + 1];
      utf8_lookup[codepoint] = to;

      const int code32 = ((unsigned char)l[i] & 0b0001'1111)
        | (((unsigned char)l[i + 1] & 0b0011'1111) << 5);

      utf32_lookup[code32] = to;
    }
  }

  return std::unique_ptr<UnicodeCollator>(new UnicodeCollator(utf8_lookup, utf32_lookup));
}

std::string UnicodeCollator::to_plain_ascii(const std::string& value) const 
{
  std::string out;
  size_t len = value.length();
  size_t i = 0;
  while (i < len) {
    unsigned char c = value[i];
    if (c > 127) {
      out.push_back('?');
      if (c < 224) i += 2;
      else if (c < 240) i += 3;
      else i += 4;
    } else {
      out.push_back(c);
      ++i;
    }
  }
  return out;
}

bool UnicodeCollator::comp(const std::string_view& value, size_t& index, const char c) const
{
  const unsigned char first = value[index];
  char n = utf8_lookup[first];
  if (first > 127) {
    if (first < 224) {
      n = utf32_lookup[(first & 0b0001'1111) + (((unsigned char)(value[++index])) << 5)];
      if (n == 0) n = '?';
    }
    else {
      index += (first < 240) ? 3 : 2;
      n = '?';
    }
  }

  return n == c;
}

bool UnicodeCollator::fuzzy_find(const std::string_view& value, const std::string& str) const {
  const size_t value_count = value.length();
  const size_t str_count = str.length();

  if (str_count > value_count) return false;
  if (str_count == 0) return true;

  const char first = str[0];
  const size_t max = value_count - str_count;

  for (size_t i = 0; i <= max; ++i) {
    if (!this->comp(value, i, first)) {
      while (++i <= max && !this->comp(value, i, first));
    }

    if (i <= max) {
      size_t j = i + 1;
      const size_t true_end = j + str_count - 1;
      const size_t end = (value_count > true_end) ? true_end : value_count;
      for (size_t k = 1; j < end && this->comp(value, j, str[k]); ++j, ++k);
      if (j == true_end) return true;
    }
  }

  return false;
}