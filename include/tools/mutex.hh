#pragma once

#include <atomic>
#include <coroutine>
#include <queue>

#include "../common.hh"
#include "../coro.hh"
#include "../task.hh"

namespace birdsong {

/* async incorporated cooperatively scheduled mutex
 * potentially a little slower than just using a traditional mutex.e */
class AsyncMutex
{
  struct LockAwaitable
  {
    LockAwaitable(AsyncMutex& mutex);

    bool await_ready();
    void await_suspend(std::coroutine_handle<> handle);
    AsyncMutex& mutex;
  };

public:
  LockAwaitable lock();
  void unlock();

private:
  std::atomic<bool> m_flag{ false };
  Mutex m_queueMutex;
  std::queue<Waker> waiting;
};

template<typename T>
class AsyncMutexWrapper
{
public:
  Coro<Empty> with_lock(std::invocable<T&> auto lambda) &&
  {
    co_await m_mutex.lock();
    auto&& out = lambda(m_data);
    m_mutex.unlock();
    co_return out;
  }

private:
  AsyncMutex m_mutex;
  T m_data;
};

};
