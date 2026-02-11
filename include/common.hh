#pragma once

#include <atomic>
#include <concepts>
#include <coroutine>
#include <type_traits>

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
  static auto await_ready() -> bool;

  /* by default, on suspend, the currently executing coroutine
   * will be cooperatively scheduled later. shadow this if you
   * intend to interface with the task system. */
  static void await_suspend(std::coroutine_handle<>);
  static auto await_resume() -> Empty;
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
  auto is_locked() -> bool;

  /* returns false if unable to lock */
  auto try_lock() -> bool;

private:
  std::atomic_flag m_flag{ false };
  unsigned m_threadLocked{ -2U };
};

class MutexLock
{
public:
  MutexLock(const MutexLock&) = delete;
  MutexLock(MutexLock&&) = delete;
  explicit MutexLock(Mutex* mutex)
    : mutex(mutex)
  {
    mutex->lock();
  }

  auto operator=(const MutexLock&) -> MutexLock& = delete;
  auto operator=(MutexLock&&) -> MutexLock& = delete;

  ~MutexLock() { mutex->unlock(); }

private:
  Mutex* mutex;
};

template<typename T>
class MutexWrapper
{
public:
  MutexWrapper() = default;

  explicit MutexWrapper(T in)
    requires std::move_constructible<T>
    : m_data(std::move(in)) {};

  auto with_lock(std::invocable<T&> auto lambda)
  {
    MutexLock lock(&m_mutex);
    return lambda(m_data);
  };

private:
  Mutex m_mutex;
  T m_data;
};

};
