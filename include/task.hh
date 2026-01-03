#pragma once

#include <list>
#include <memory>

#include "atomic.hh"
#include "common.hh"
#include "coro.hh"

namespace birdsong {

class Runtime;
class Task;

class Waker : public Atom
{
public:
  Waker(Runtime& runtime, std::unique_ptr<Task> task);

  Waker(Waker&&);

  /* NOTE: will attempt to lock the task it owns!
   * if you have any transactional locks on the task,
   * make sure you drop em before calling wake */
  void wake();

  using Data = std::unique_ptr<Task>;
  Data& get_data(Atom::Key) { return task; }

private:
  /* TODO: i can probably use a memory pool for tasks */
  Runtime& runtime;
  Data task;
};

struct SharedTaskStateBase
{
  SharedTaskStateBase(Task const& dependent)
    : dependent(dependent) {};

  Task const& dependent;

  /* any JoinHandles of a task that are co_await'd have
   * the parent task slept and the wakers are added here.
   * when the dependent task destructs, these are all .wake()'d */
  std::list<Waker> join_handle_wakers;

  /* when a tasks killswitch is active, the task will no longer
   * be able to be woken. this is equivalent to terminating
   * a thread at its suspension point. */
  bool killswitch{ false };

  Mutex mutex;
};

template<typename T>
struct SharedTaskStateRet : public SharedTaskStateBase
{
  std::optional<T> retval;
};

/* TODO: mark if a task is currently being executed by a given
 * thread, so that multiple concurrent executions cannot happen.
 * logically, no task should be executed by multiple threads
 * but if a task is woken by a waker whilst some other
 * code that was managing the waker within said task hasnt yet ended
 * it could occur that multiple threads try to run a task. */
class Task : public Atom
{
public:
  struct Data
  {
    /* current suspension point
     * references cannot be rebound, so it must be a pointer
     */
    std::coroutine_handle<PromiseBase> handle;
    std::atomic<std::shared_ptr<SharedTaskStateBase>> state;
  };

  Task(Runtime& rt, PromiseBase* promise);

  Task(const Task&) = delete;
  Task(Task&&) = delete;
  Task& operator=(const Task&) = delete;
  Task& operator=(Task&&) = delete;
  virtual ~Task();

  bool operator==(Task const& rhs) const { return this == &rhs; };

  Data& get_data(Atom::Key) { return m_data; };

  unsigned tag;

private:
  Data m_data;
};

template<typename T>
class TaskSpecified final : public Task
{
public:
  using Task::Task;

  ~TaskSpecified()
  {
    auto transaction = acquire();

    /* if and only if the coroutine promise is the topmost
     * coroutine frame and the coroutine is done executing,
     * set the retval on the joinhandle */
    if (transaction->handle.done() and
        transaction->handle.promise().parent == nullptr)
      ((SharedTaskStateRet<T>*)transaction->state.load().get())->retval =
        std::move(
          static_cast<Coro<T>::promise_type&>(transaction->handle.promise())
            .retval);
  }
};

class JoinHandleBase
{
public:
  JoinHandleBase(Runtime& rt, Task& dependent)
    : rt(rt)
    , m_state(dependent.acquire()->state.load()) {};

  JoinHandleBase(const JoinHandleBase&) = delete;
  JoinHandleBase(JoinHandleBase&& rhs)
    : rt(rhs.rt)
    , m_state(rhs.m_state.load()) {};

  JoinHandleBase& operator=(const JoinHandleBase&) = delete;
  JoinHandleBase& operator=(JoinHandleBase&&) = delete;

  void await_suspend(std::coroutine_handle<>);
  Task const& get_task() { return m_state.load()->dependent; }
  void kill();

protected:
  Runtime& rt;
  std::atomic<std::shared_ptr<SharedTaskStateBase>> m_state;
};

/* ugh i can clean this up later.
 * for now, DONT TOUCH ANYTHING */
template<typename T>
class JoinHandle : public JoinHandleBase
{
public:
  JoinHandle(const JoinHandle&) = delete;
  JoinHandle(JoinHandle&& rhs) = default;
  JoinHandle& operator=(const JoinHandle&) = delete;
  JoinHandle& operator=(JoinHandle&&) = delete;

  using JoinHandleBase::JoinHandleBase;

  bool await_ready()
  {
    get_state().mutex.lock();
    return (this->ready = get_state().retval.has_value());
  }

  T await_resume()
  {
    if (!ready)
      get_state().mutex.lock();

    T out = get_state().retval.value();
    get_state().mutex.unlock();
    return std::move(out);
  }

private:
  SharedTaskStateRet<T>& get_state()
  {
    return static_cast<SharedTaskStateRet<T>&>(*m_state.load().get());
  };

  bool ready{ false };
};

};
