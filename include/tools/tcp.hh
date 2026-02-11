#pragma once

#include <compare>
#include <concepts>
#include <coroutine>
#include <cstddef>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <type_traits>
#include <utility>

#include "../common.hh"
#include "../coro.hh"
#include "../io.hh"
#include "../net.hh"

namespace birdsong {

class TCPSocket
{
  class Read
  {
  public:
    Read(TCPSocket* socket, std::span<std::byte> buf)
      : socket(socket)
      , buf(buf) {};

    auto await_ready() -> bool;
    auto await_suspend(std::coroutine_handle<> /*unused*/) -> void;
    auto await_resume() -> IOResult<unsigned>;

  private:
    TCPSocket* socket;
    std::span<std::byte> buf;
  };

  class Write
  {
  public:
    Write(TCPSocket* socket, std::span<const std::byte> buf)
      : socket(socket)
      , buf(buf) {};

    auto await_ready() -> bool;
    auto await_suspend(std::coroutine_handle<> /*unused*/) -> void;
    auto await_resume() -> IOResult<unsigned>;

  private:
    TCPSocket* socket;
    std::span<std::byte const> buf;
  };

  class Connect
  {
  public:
    Connect(unsigned int m_fd, unsigned int m_addr, unsigned short m_port)
      : m_fd(m_fd)
      , m_addr(m_addr)
      , m_port(m_port) {};

    auto await_ready() -> bool;
    void await_suspend(std::coroutine_handle<>);
    auto await_resume() -> std::optional<TCPSocket>;

  private:
    signed m_fd;
    unsigned m_addr;
    unsigned short m_port;
  };

public:
  TCPSocket(signed fd, IPAddr addr);
  ~TCPSocket();

  TCPSocket(const TCPSocket&) = delete;
  auto operator=(const TCPSocket&) -> TCPSocket& = delete;
  TCPSocket(TCPSocket&&) noexcept;
  auto operator=(TCPSocket&&) noexcept -> TCPSocket&;

  static auto connect(Runtime&, unsigned short port, uint32_t address)
    -> Connect;

  auto read(std::span<std::byte> buffer) -> Read;
  auto write(std::span<std::byte const> buffer) -> Write;

  [[nodiscard]] auto addr() const -> IPAddr const&;

private:
  signed m_fd{ -1 };
  IPAddr m_addr;
};

class TCPListener
{
  class AcceptAwaiter
  {
  public:
    explicit AcceptAwaiter(TCPListener* listener);

    auto await_ready() -> bool;
    void await_suspend(std::coroutine_handle<>);

    /* if a connection is aborted in the process of accepting,
     * then this function can return nullopt */
    auto await_resume() -> std::optional<TCPSocket>;

  private:
    TCPListener* listener;
  };

public:
  TCPListener(const TCPListener&) = default;
  TCPListener(TCPListener&&) = delete;
  auto operator=(const TCPListener&) -> TCPListener& = default;
  auto operator=(TCPListener&&) -> TCPListener& = delete;

  auto constexpr static default_queue_size = 16;
  explicit TCPListener(unsigned short port,
                       signed queue_size = default_queue_size);
  ~TCPListener();

  /* asynchronously blocks this thread and awaits
   * an incoming connection */
  auto accept() -> AcceptAwaiter;

private:
  int m_fd{ -1 };
};

};
