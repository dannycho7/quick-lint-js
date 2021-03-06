// quick-lint-js finds bugs in JavaScript programs.
// Copyright (C) 2020  Matthew Glazar
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include <array>
#include <gtest/gtest.h>
#include <limits>
#include <quick-lint-js/integer.h>

namespace quick_lint_js {
namespace {
template <class T>
string8 write_integer(T value) {
  std::array<char8, integer_string_length<T>> chars;
  std::fill(chars.begin(), chars.end(), 'x');
  char8* end = quick_lint_js::write_integer(value, chars.data());
  EXPECT_LE(end - chars.data(), chars.size());
  return string8(chars.data(), end);
}

TEST(test_write_integer, common_integers) {
  EXPECT_EQ(write_integer(std::size_t{0}), u8"0");
  EXPECT_EQ(write_integer(std::size_t{1234}), u8"1234");
  EXPECT_EQ(write_integer(std::size_t{1234}), u8"1234");
}

TEST(test_write_integer, maximum) {
  if constexpr (std::numeric_limits<std::size_t>::max() >= 4294967295ULL) {
    EXPECT_EQ(write_integer(std::size_t{4294967295ULL}), u8"4294967295");
  }
  if constexpr (std::numeric_limits<std::size_t>::max() >=
                18446744073709551615ULL) {
    EXPECT_EQ(write_integer(std::size_t{18446744073709551615ULL}),
              u8"18446744073709551615");
  }
}
}
}
