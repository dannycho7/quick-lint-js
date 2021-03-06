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

#ifndef QUICK_LINT_JS_INTEGER_H
#define QUICK_LINT_JS_INTEGER_H

#include <cstddef>
#include <limits>
#include <quick-lint-js/char8.h>
#include <system_error>

namespace quick_lint_js {
struct from_chars_result {
  const char *ptr;
  std::errc ec;
};

from_chars_result from_chars(const char *begin, const char *end, int &value);
from_chars_result from_chars_hex(const char *begin, const char *end,
                                 int &value);

template <class T>
inline constexpr int integer_string_length =
    (std::numeric_limits<T>::digits10 + 1) +
    (std::numeric_limits<T>::is_signed ? 1 : 0);

template <class T>
char8 *write_integer(T, char8 *out);

extern template char8 *write_integer<std::size_t>(std::size_t, char8 *out);
}

#endif
