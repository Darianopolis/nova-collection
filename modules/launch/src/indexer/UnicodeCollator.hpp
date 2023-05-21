#pragma once
#ifndef UNICODE_COLLATOR_H
#define UNICODE_COLLATOR_H

#include <string>
#include <string_view>
#include <cctype>
#include <iostream>
#include <vector>
#include <memory>

class UnicodeCollator
{
public:
  inline bool comp(const std::string_view& value, size_t& index, const char c) const;
  bool fuzzy_find(const std::string_view& value, const std::string& str) const;
  std::string to_plain_ascii(const std::string& value) const;

  static std::unique_ptr<UnicodeCollator> new_ascii_collator();
  
private:
  std::vector<char> utf8_lookup;
  std::vector<char> utf32_lookup;

  UnicodeCollator(const std::vector<char> utf8_lookup, const std::vector<char> utf32_lookup);
};

#endif // !UNICODE_COLLATOR_H
