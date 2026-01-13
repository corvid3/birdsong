#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <coroutine>
#include <cstring>
#include <expected>
#include <format>
#include <netinet/in.h>
#include <optional>
#include <poll.h>
#include <sys/poll.h>
#include <sys/socket.h>

#include "coro.hh"
#include "reactor.hh"
#include "runtime.hh"
#include "task.hh"
#include "tools/tcp.hh"

using namespace birdsong;

static void
setnonblock(unsigned fd)
{
  static unsigned val = true;
  setsockopt(fd, SOL_SOCKET, SOCK_NONBLOCK, &val, sizeof val);
}

TCPListener::TCPListener(unsigned short port, unsigned queue_size)
{
  m_fd = socket(AF_INET, SOCK_STREAM, 0);

  if (m_fd == -1u)
    throw std::runtime_error("unable to create tcp listener");

  int val = 1;
  setsockopt(m_fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof val);

  struct sockaddr_in addr;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(port);
  memset(addr.sin_zero, 0, sizeof addr.sin_zero);
  addr.sin_family = AF_INET;

  if (bind(m_fd, (struct sockaddr const*)&addr, sizeof addr) < 0)
    throw std::runtime_error(
      std::format("unable to bind tcp listener {}", strerror(errno)));

  if (listen(m_fd, queue_size) < 0)
    throw std::runtime_error("unable to listen tcp listener socket");
}

TCPListener::~TCPListener()
{
  if (m_fd != -1u)
    close(m_fd);
}

TCPListener::AcceptAwaiter::AcceptAwaiter(TCPListener& listener)
  : listener(listener) {};

auto
TCPListener::accept() -> AcceptAwaiter
{
  return { *this };
}

bool
TCPListener::AcceptAwaiter::await_ready()
{
  /* do an in-place non-blocking poll to check if
   * the socket already has an incoming connection,
   * if so just continue the coroutine */
  struct pollfd pfd;
  pfd.fd = listener.m_fd;
  pfd.events = 0 | POLLIN;

  if (poll(&pfd, 1, 0) < 0)
    throw std::runtime_error("fatal poll error");

  return pfd.revents & POLLIN;
}

void
TCPListener::AcceptAwaiter::await_suspend(std::coroutine_handle<> handle)
{
  Runtime& rt = basic_handle_from_void(handle).promise().runtime;
  Reactor& reactor = rt.get_reactor();
  reactor.insert(
    Reactor::FDWait{ rt.create_waker(), listener.m_fd, { true, false } });
}

std::optional<TCPSocket>
TCPListener::AcceptAwaiter::await_resume()
{
  int incoming_fd;

  if ((incoming_fd = ::accept(listener.m_fd, NULL, NULL)) == -1)
    return std::nullopt;
  setnonblock(incoming_fd);

  return TCPSocket(incoming_fd);
}

TCPSocket::TCPSocket(unsigned fd)
  : m_fd(fd) {};

TCPSocket::~TCPSocket()
{
  if (m_fd != -1u)
    close(m_fd);
}

TCPSocket::TCPSocket(TCPSocket&& rhs)
{
  this->m_fd = rhs.m_fd;
  rhs.m_fd = -1u;
}

TCPSocket&
TCPSocket::operator=(TCPSocket&& rhs)
{
  return *new (this) TCPSocket(std::move(rhs));
}

auto
TCPSocket::connect(Runtime&, unsigned short port, uint32_t address) -> Connect
{
  return Connect{ -1u, address, port };
}

auto
TCPSocket::read(std::span<std::byte> buffer) -> Read
{
  return Read(*this, buffer);
}

auto
TCPSocket::write(std::span<std::byte const> buffer) -> Write
{
  return Write(*this, buffer);
}

bool
TCPSocket::Read::await_ready()
{
  struct pollfd pfd;
  pfd.fd = socket.m_fd;
  pfd.events = POLLIN;
  return poll(&pfd, 1, 0) != 0;
}

void
TCPSocket::Read::await_suspend(std::coroutine_handle<> handle)
{
  Runtime& rt = basic_handle_from_void(handle).promise().runtime;
  rt.get_reactor().insert({ rt.create_waker(), socket.m_fd, { true, false } });
}

std::expected<unsigned, unsigned>
TCPSocket::Read::await_resume()
{
  unsigned val = ::recv(socket.m_fd, buf.data(), buf.size(), 0);

  if (val == -1u)
    return std::unexpected(errno);
  else
    return val;
}

bool
TCPSocket::Write::await_ready()
{
  struct pollfd pfd;
  pfd.fd = socket.m_fd;
  pfd.events = POLLOUT;
  return poll(&pfd, 1, 0) != 0;
}

void
TCPSocket::Write::await_suspend(std::coroutine_handle<> handle)
{
  Runtime& rt = basic_handle_from_void(handle).promise().runtime;
  rt.get_reactor().insert({ rt.create_waker(), socket.m_fd, { false, true } });
}

std::expected<unsigned, unsigned>
TCPSocket::Write::await_resume()
{
  /* SIGPIPE is weird and ugly. don't send it. */
  unsigned val = ::send(socket.m_fd, buf.data(), buf.size(), MSG_NOSIGNAL);

  if (val == -1u)
    return std::unexpected(errno);
  else
    return val;
}

bool
TCPSocket::Connect::await_ready()
{
  m_fd = socket(AF_INET, SOCK_STREAM, 0);

  /* failure to create a socket in the first place is pretty exceptional */
  if (m_fd == -1u)
    throw std::runtime_error("unable to create tcp socket\n");

  setnonblock(m_fd);
  struct sockaddr_in addr{};
  addr.sin_addr.s_addr = htonl(m_addr);
  addr.sin_port = htons(m_port);
  addr.sin_family = AF_INET;
  if (::connect(m_fd, (struct sockaddr*)&addr, sizeof addr) < 0)
    return (close(m_fd), m_fd = -1, true);

  pollfd pfd;
  pfd.fd = m_fd;
  pfd.events = POLLOUT;
  if (::poll(&pfd, 1, 0) < 0)
    throw std::runtime_error("fatal poll error");
  return pfd.revents & POLLOUT;
}

void
TCPSocket::Connect::await_suspend(std::coroutine_handle<> handle)
{
  Runtime& rt = basic_handle_from_void(handle).promise().runtime;
  rt.get_reactor().insert({ rt.create_waker(), m_fd, { false, true } });
}

std::optional<TCPSocket>
TCPSocket::Connect::await_resume()
{
  if (m_fd == -1u)
    return std::nullopt;
  return TCPSocket(m_fd);
}
