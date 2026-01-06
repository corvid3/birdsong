#pragma once

#include <atomic>
#include <concepts>
#include <coroutine>

namespace birdsong {

/* dealing with void return types in coroutines is _really_ annoying
 * for like no reason whatsoever. so i just use an empty
 * type in place of void */
struct Empty
{};

/* any awaiter interfacing with the birdsong runtime system
 * MUST derive from this base class. */
class AwaitableBase
{
public:
  /* by default, all awaiters are lazily evaluated.
   * override this is you want to check inline */
  bool await_ready();
  /* by default, on suspend, the currently executing coroutine
   * will be cooperatively scheduled later. shadow this if you
   * intend to interface with the task system. */
  void await_suspend(std::coroutine_handle<>);
  Empty await_resume();
};

/* general use spinlock-based mutex.
 * does not yield on lock().
 * if your task is not going to lock for very long,
 * use a traditional mutex. if your task is likely
 * going to lock for a while, use the Async mutex. */
class Mutex
{
  /* implemented in tools/mutex.cc btw */
public:
  void lock();
  void unlock();
  bool is_locked();

  /* returns false if unable to lock */
  bool try_lock();

private:
  std::atomic_flag m_flag{ false };
  unsigned m_threadLocked{ -2u };
};

class MutexLock
{
public:
  MutexLock(Mutex& mutex)
    : mutex(mutex)
  {
    mutex.lock();
  }

  ~MutexLock() { mutex.unlock(); }

private:
  Mutex& mutex;
};

template<typename T>
class MutexWrapper
{
public:
  MutexWrapper() = default;
  MutexWrapper(T in)
    : m_data(std::move(in)) {};

  auto with_lock(std::invocable<T&> auto lambda)
  {
    MutexLock lock(m_mutex);
    return lambda(m_data);
  };

private:
  Mutex m_mutex;
  T m_data;
};

};
