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

#include <cstddef>
#include <cstring>
#include <future>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <quick-lint-js/char8.h>
#include <quick-lint-js/file-handle.h>
#include <quick-lint-js/file.h>
#include <quick-lint-js/have.h>
#include <quick-lint-js/lsp-pipe-writer.h>
#include <quick-lint-js/narrow-cast.h>
#include <quick-lint-js/pipe.h>
#include <thread>

#if QLJS_HAVE_PTHREAD_KILL
#include <pthread.h>
#include <signal.h>
#endif

#if QLJS_HAVE_FCNTL_H
#include <fcntl.h>
#endif

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using namespace std::literals::chrono_literals;

namespace quick_lint_js {
namespace {
std::size_t pipe_buffer_size(platform_file_ref);

#if QLJS_HAVE_PTHREAD_KILL
class sigaction_guard {
 public:
  explicit sigaction_guard(int signal_number) : signal_number_(signal_number) {
    int rc =
        ::sigaction(this->signal_number_, nullptr, &this->saved_sigaction_);
    QLJS_ALWAYS_ASSERT(rc == 0);
  }

  ~sigaction_guard() {
    int rc =
        ::sigaction(this->signal_number_, &this->saved_sigaction_, nullptr);
    QLJS_ALWAYS_ASSERT(rc == 0);
  }

  sigaction_guard(const sigaction_guard &) = delete;
  sigaction_guard &operator=(const sigaction_guard &) = delete;

 private:
  int signal_number_;
  struct sigaction saved_sigaction_;
};
#endif

class test_lsp_pipe_writer : public ::testing::Test {
 public:
  pipe_fds pipe = make_pipe();
  lsp_pipe_writer writer{this->pipe.writer.ref()};
};

TEST_F(test_lsp_pipe_writer, small_message_includes_content_length) {
  this->writer.send_message(u8"hi");
  this->pipe.writer.close();

  read_file_result data = read_file("<pipe>", this->pipe.reader.ref());
  ASSERT_TRUE(data.ok()) << data.error;
  EXPECT_EQ(data.content, u8"Content-Length: 2\r\n\r\nhi");
}

TEST_F(test_lsp_pipe_writer, large_message_sends_fully) {
  std::future<read_file_result> data_future = std::async(
      std::launch::async,
      [this]() { return read_file("<pipe>", this->pipe.reader.ref()); });

  string8 message =
      u8"[" + string8(pipe_buffer_size(this->pipe.writer.ref()) * 3, u8'x') +
      u8"]";
  this->writer.send_message(message);
  this->pipe.writer.close();

  read_file_result data = data_future.get();
  ASSERT_TRUE(data.ok()) << data.error;

  string8_view data_content = data.content.string_view();
  EXPECT_NE(data_content.find(message), data_content.npos);
}

#if QLJS_HAVE_PTHREAD_KILL
TEST_F(test_lsp_pipe_writer, large_message_sends_fully_with_interrupt) {
  sigaction_guard signal_guard(SIGALRM);

  ::pthread_t writer_thread_id = ::pthread_self();

  ASSERT_NE(::signal(SIGALRM,
                     [](int) {
                       // Do nothing. Just interrupt syscalls.
                     }),
            SIG_ERR)
      << std::strerror(errno);

  std::future<read_file_result> data_future =
      std::async(std::launch::async, [this, writer_thread_id]() {
        int rc;

        // Interrupt the write() syscall, causing it to return early.
        std::this_thread::sleep_for(10ms);  // Wait for write() to execute.
        rc = ::pthread_kill(writer_thread_id, SIGALRM);
        EXPECT_EQ(rc, 0) << std::strerror(rc);
        // The pipe's buffer should now be full.

        // Interrupt the write() syscall again, causing it to restart. This
        // write() call shouldn't have written anything, because the pipe's
        // buffer is already full.
        std::this_thread::sleep_for(1ms);  // Wait for write() to execute.
        rc = ::pthread_kill(writer_thread_id, SIGALRM);
        EXPECT_EQ(rc, 0) << std::strerror(rc);

        return read_file("<pipe>", this->pipe.reader.ref());
      });

  string8 message =
      u8"[" + string8(pipe_buffer_size(this->pipe.writer.ref()) * 3, u8'x') +
      u8"]";
  this->writer.send_message(message);
  this->pipe.writer.close();

  read_file_result data = data_future.get();
  ASSERT_TRUE(data.ok()) << data.error;

  string8_view data_content = data.content.string_view();
  EXPECT_NE(data_content.find(message), data_content.npos);
}
#endif

#if QLJS_HAVE_PTHREAD_KILL
TEST_F(test_lsp_pipe_writer,
       large_message_sends_fully_with_interrupt_without_syscall_restart) {
  sigaction_guard signal_guard(SIGALRM);

  ::pthread_t writer_thread_id = ::pthread_self();

  {
    struct sigaction act;
    act.sa_sigaction = [](int, siginfo_t *, void *) {
      // Do nothing. Just interrupt syscalls.
    };
    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_SIGINFO;
    EXPECT_FALSE(act.sa_flags & SA_RESTART) << "SA_RESTART should be unset";
    ASSERT_EQ(sigaction(SIGALRM, &act, nullptr), 0) << std::strerror(errno);
  }

  std::future<read_file_result> data_future =
      std::async(std::launch::async, [this, writer_thread_id]() {
        int rc;

        // Interrupt the write() syscall, causing it to return early.
        std::this_thread::sleep_for(10ms);  // Wait for write() to execute.
        rc = ::pthread_kill(writer_thread_id, SIGALRM);
        EXPECT_EQ(rc, 0) << std::strerror(rc);
        // The pipe's buffer should now be full.

        // Interrupt the write() syscall again. This write() call shouldn't have
        // written anything, because the pipe's buffer is already full. write()
        // would return EINTR because SA_RESTART was not set for SIGALRM.
        std::this_thread::sleep_for(1ms);  // Wait for write() to execute.
        rc = ::pthread_kill(writer_thread_id, SIGALRM);
        EXPECT_EQ(rc, 0) << std::strerror(rc);

        return read_file("<pipe>", this->pipe.reader.ref());
      });

  string8 message =
      u8"[" + string8(pipe_buffer_size(this->pipe.writer.ref()) * 3, u8'x') +
      u8"]";
  this->writer.send_message(message);
  this->pipe.writer.close();

  read_file_result data = data_future.get();
  ASSERT_TRUE(data.ok()) << data.error;

  string8_view data_content = data.content.string_view();
  EXPECT_NE(data_content.find(message), data_content.npos);
}
#endif

std::size_t pipe_buffer_size([[maybe_unused]] platform_file_ref pipe) {
#if QLJS_HAVE_F_GETPIPE_SZ
  int size = ::fcntl(pipe.get(), F_GETPIPE_SZ);
  EXPECT_NE(size, -1);
  return narrow_cast<std::size_t>(size);
#elif defined(__APPLE__)
  // See BIG_PIPE_SIZE in <xnu>/bsd/sys/pipe.h.
  return 65536;
#elif defined(_WIN32)
  DWORD outBufferSize = 0;
  EXPECT_TRUE(::GetNamedPipeInfo(pipe.get(), PIPE_CLIENT_END | PIPE_TYPE_BYTE,
                                 &outBufferSize, /*lpInBufferSize=*/nullptr,
                                 /*lpMaxInstances=*/nullptr))
      << windows_handle_file::get_last_error_message();
  return outBufferSize;
#else
#error "Unknown platform"
#endif
}
}
}
