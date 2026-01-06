#pragma once

#include <concepts>
#include <expected>
#include <span>

#include "coro.hh"

namespace birdsong {

using Errno = unsigned;

template<typename T>
using IOResult = std::expected<T, Errno>;

template<typename T>
concept AsyncWriter = requires(T t, std::span<std::byte const> buf) {
  { t.write(buf).await_resume() } -> std::same_as<IOResult<unsigned>>;
};

template<typename T>
concept AsyncReader = requires(T t, std::span<std::byte> buf) {
  { t.read(buf).await_resume() } -> std::same_as<IOResult<unsigned>>;
};

Coro<IOResult<unsigned>>
write_all(Runtime&, AsyncWriter auto& writer, std::span<std::byte const> buf)
{
  unsigned idx = 0;

  do {
    auto const res = co_await writer.write({ buf.begin() + idx, buf.end() });
    if (res)
      if (*res == 0)
        break;
      else
        idx += *res;
    else
      co_return std::unexpected(res.error());
  } while (idx != buf.size_bytes());

  co_return buf.size_bytes();
}

Coro<IOResult<unsigned>>
read_all(Runtime&, AsyncReader auto& reader, std::span<std::byte> buf)
{
  unsigned idx = 0;

  do {
    auto const res = co_await reader.read({ buf.begin() + idx, buf.end() });
    if (res)
      if (*res == 0)
        break;
      else
        idx += *res;
    else
      co_return std::unexpected(res.error());
  } while (idx != buf.size_bytes());

  co_return buf.size_bytes();
}

};
