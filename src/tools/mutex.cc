#include <atomic>
#include <print>

#include "runtime.hh"
#include "thread_queue.hh"
#include "tools/mutex.hh"

using namespace birdsong;

void
Mutex::lock()
{
  unsigned this_thread_id = ThreadQueue::GetThisThreadID();
  if (m_threadLocked == this_thread_id)
    std::println("deadlock detected. thread: {}", m_threadLocked),
      std::terminate();

  while (m_flag.test_and_set(std::memory_order_acquire))
    m_flag.wait(true);

  m_threadLocked = this_thread_id;
}

bool
Mutex::try_lock()
{
  unsigned this_thread_id = ThreadQueue::GetThisThreadID();
  if (m_threadLocked == this_thread_id)
    std::println("deadlock detected. thread: {}", m_threadLocked),
      std::terminate();

  bool success = m_flag.test_and_set(std::memory_order_acq_rel);
  if (success)
    m_threadLocked = this_thread_id;

  return success;
}

void
Mutex::unlock()
{
  unsigned this_thread_id = ThreadQueue::GetThisThreadID();
  if (m_threadLocked != this_thread_id)
    std::println("unlock when not owning mutex. thread: {}", this_thread_id),
      std::terminate();
  m_threadLocked = -2u;
  m_flag.clear(std::memory_order_release);
  m_flag.notify_one();
}

bool
Mutex::is_locked()
{
  return m_flag.test(std::memory_order::acquire);
}

AsyncMutex::LockAwaitable::LockAwaitable(AsyncMutex& mutex)
  : mutex(mutex) {};

bool
AsyncMutex::LockAwaitable::await_ready()
{
  return mutex.m_queueMutex.is_locked();
}

void
AsyncMutex::LockAwaitable::await_suspend(std::coroutine_handle<> handle)
{
  auto runtime = basic_handle_from_void(handle).promise().runtime;

  mutex.m_queueMutex.lock();
  mutex.waiting.push(runtime->create_waker());
  mutex.m_queueMutex.unlock();
}

AsyncMutex::LockAwaitable
AsyncMutex::lock()
{
  return { *this };
};

void
AsyncMutex::unlock()
{
  /* store false so that one of the spinlocks breaks
   * and then wake up one of the threads */

  m_queueMutex.lock();
  waiting.pop();
  m_queueMutex.unlock();
  m_flag.store(false);
}
