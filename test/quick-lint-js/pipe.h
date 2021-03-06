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

#ifndef QUICK_LINT_JS_PIPE_H
#define QUICK_LINT_JS_PIPE_H

#include <quick-lint-js/file-handle.h>
#include <quick-lint-js/have.h>

namespace quick_lint_js {
#if QLJS_HAVE_PIPE
struct pipe_fds {
  posix_fd_file reader;
  posix_fd_file writer;
};

pipe_fds make_pipe();
#endif
}

#endif
