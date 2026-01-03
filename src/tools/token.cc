#include <coroutine>
#include <list>

#include "coro.hh"
#include "priv_runtime.hh"
#include "task.hh"
#include "tools/token.hh"

using namespace birdsong;

struct Token::Impl
{
  Mutex mutex;
  std::atomic<bool> flag{ false };
  std::list<Waker> waiting;
};

Token::Token()
  : m_impl(new Impl) {};

Token::
operator bool() const
{
  return m_impl->flag.load();
};

bool
Token::await_ready()
{
  m_impl->mutex.lock();
  if (m_impl->flag)
    return m_impl->mutex.unlock(), true;
  else
    return false;
}

void
Token::await_suspend(std::coroutine_handle<> handle)
{
  auto& rt = basic_handle_from_void(handle).promise().runtime;
  m_impl->waiting.push_back(rt.create_waker());
  m_impl->mutex.unlock();
}

void
Token::go()
{
  m_impl->mutex.lock();

  if (m_impl->flag != true) {
    m_impl->flag = true;
    for (auto& waker : m_impl->waiting) {
      waker.wake();
    }
  }

  m_impl->mutex.unlock();
}
