#pragma once

#include <coroutine>
#include <cstddef>
#include <expected>
#include <optional>
#include <span>

#include "../common.hh"
#include "../coro.hh"
#include "../io.hh"

/* im _not_ trying to build a cross-platform networking
 * library here, so some of the internal posix networking
 * datatypes might leak out. */

namespace birdsong {

class TCPSocket
{
  struct Read : AwaitableBase
  {
    Read(TCPSocket& socket, std::span<std::byte> buf)
      : socket(socket)
      , buf(buf) {};

    void await_suspend(std::coroutine_handle<>);
    std::expected<unsigned, unsigned> await_resume();

    TCPSocket& socket;
    std::span<std::byte> buf;
  };

  struct Write : AwaitableBase
  {
    Write(TCPSocket& socket, std::span<const std::byte> buf)
      : socket(socket)
      , buf(buf) {};

    void await_suspend(std::coroutine_handle<>);
    IOResult<unsigned> await_resume();

    TCPSocket& socket;
    std::span<std::byte const> buf;
  };

  struct Connect : AwaitableBase
  {
    Connect(unsigned int m_fd, unsigned int m_addr, unsigned short m_port)
      : m_fd(m_fd)
      , m_addr(m_addr)
      , m_port(m_port) {};

    bool await_ready();
    void await_suspend(std::coroutine_handle<>);
    std::optional<TCPSocket> await_resume();

    unsigned m_fd;
    unsigned m_addr;
    unsigned short m_port;
  };

public:
  TCPSocket(unsigned fd);
  ~TCPSocket();

  TCPSocket(const TCPSocket&) = delete;
  TCPSocket& operator=(const TCPSocket&) = delete;

  TCPSocket(TCPSocket&&);
  TCPSocket& operator=(TCPSocket&&);

  static Connect connect(Runtime&, unsigned short port, uint32_t address);

  Read read(std::span<std::byte> buffer);
  Write write(std::span<std::byte const> buffer);

private:
  unsigned m_fd = -1u;
};

class TCPListener
{
  class AcceptAwaiter
  {
  public:
    AcceptAwaiter(TCPListener& listener);

    bool await_ready();
    void await_suspend(std::coroutine_handle<>);

    /* if a connection is aborted in the process of accepting,
     * then this function can return nullopt */
    std::optional<TCPSocket> await_resume();

  private:
    TCPListener& listener;
  };

public:
  struct Options
  {
    bool reuseaddr = false;
    int max_incoming = 16;
  };

  TCPListener(unsigned short port, Options const&);
  ~TCPListener();

  /* asynchronously blocks this thread and awaits
   * an incoming connection */
  AcceptAwaiter accept();

private:
  unsigned m_fd = -1u;
};

};
