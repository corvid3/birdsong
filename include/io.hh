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

template<typename T>
concept AsyncIO = AsyncReader<T> and AsyncWriter<T>;

class PolyIOBase
{
public:
  PolyIOBase() = default;
  PolyIOBase(const PolyIOBase&) = default;
  PolyIOBase(PolyIOBase&&) = default;
  auto operator=(const PolyIOBase&) -> PolyIOBase& = default;
  auto operator=(PolyIOBase&&) -> PolyIOBase& = default;

  virtual ~PolyIOBase() = default;

  virtual auto read(std::span<std::byte> buf) -> Coro<IOResult<unsigned>> = 0;
  virtual auto write(std::span<std::byte> buf) -> Coro<IOResult<unsigned>> = 0;
};

template<typename Derived>
  requires AsyncIO<Derived>
class PolyIO final
  : PolyIOBase
  , Derived
{
public:
  explicit PolyIO(Derived d)
    : Derived(std::move(d)) {};
  ~PolyIO() override = default;
  PolyIO(const PolyIO&) = default;
  PolyIO(PolyIO&&) = default;
  auto operator=(const PolyIO&) -> PolyIO& = default;
  auto operator=(PolyIO&&) -> PolyIO& = default;

  auto read(std::span<std::byte> buf) -> Coro<IOResult<unsigned>> final
  {
    auto* d = static_cast<Derived*>(this);
    co_return co_await d->read(buf);
  }

  auto write(std::span<std::byte> buf) -> Coro<IOResult<unsigned>> final
  {
    auto* d = static_cast<Derived*>(this);
    co_return co_await d->write(buf);
  }
};

template<typename T>
  requires AsyncIO<T>
auto inline make_polyio(T&& in)
{
  return PolyIO<T>(std::forward<T>((in)));
}

auto
write_all(AsyncWriter auto* writer, std::span<std::byte const> buf)
  -> Coro<IOResult<unsigned>>
{
  unsigned idx = 0;

  while (idx != buf.size_bytes()) {
    auto const res = co_await writer->write({ buf.begin() + idx, buf.end() });
    if (res) {
      if (*res == 0)
        break;

      idx += *res;
    } else {
      co_return std::unexpected(res.error());
    }
  }

  co_return buf.size_bytes();
}

auto
read_all(AsyncReader auto* reader, std::span<std::byte> buf)
  -> Coro<IOResult<unsigned>>
{
  unsigned idx = 0;

  while (idx != buf.size_bytes()) {
    auto const res = co_await reader->read({ buf.begin() + idx, buf.end() });

    if (res) {
      if (*res == 0)
        break;
      idx += *res;
    } else {
      co_return std::unexpected(res.error());
    }
  }

  co_return buf.size_bytes();
}

};
