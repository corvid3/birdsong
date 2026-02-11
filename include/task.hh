#pragma once

#include <concepts>
#include <list>
#include <memory>

#include "atomic.hh"
#include "common.hh"
#include "coro.hh"

namespace birdsong {

class Runtime;
class Task;

template<typename T>
concept Awaitable = requires(T t) {
  { t.await_ready() } -> std::same_as<bool>;
  t.await_suspend();
  t.await_resume();
};

template<typename Awaitable>
concept Cancellable = requires(Awaitable t) {
  { t.cancel() };
};

class Waker : public Atom
{
public:
  Waker(Waker&&) noexcept;
  ~Waker() = default;

  Waker(const Waker&) = delete;
  auto operator=(const Waker&) -> Waker& = delete;
  auto operator=(Waker&&) -> Waker& = delete;
  Waker(Runtime* runtime, std::unique_ptr<Task> task);

  /* NOTE: will attempt to lock the task it owns!
   * if you have any transactional locks on the task,
   * make sure you drop em before calling wake */
  void wake();

  using Data = std::unique_ptr<Task>;
  auto get_data(Atom::Key /*unused*/) -> Data& { return task; }

private:
  /* TODO: i can probably use a memory pool for tasks */
  Runtime* runtime;
  Data task;
};

struct SharedTaskState
{
  Task* dependent;
  Coro<> entry_coro;

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
    std::atomic<std::shared_ptr<SharedTaskState>> state;
  };

  Task(Runtime& rt, Coro<>);

  Task(const Task&) = delete;
  Task(Task&&) = delete;
  auto operator=(const Task&) -> Task& = delete;
  auto operator=(Task&&) -> Task& = delete;
  virtual ~Task();

  void kill();

  auto operator==(Task const& rhs) const -> bool { return this == &rhs; };

  auto get_data(Atom::Key /*unused*/) -> Data& { return m_data; };

  auto tag() const noexcept -> unsigned { return m_tag; };

private:
  unsigned m_tag;
  Data m_data;
};

class JoinHandleBase
{
public:
  JoinHandleBase(Runtime* rt, Task& dependent)
    : rt(rt)
    , m_state(dependent.acquire()->state.load()) {};
  JoinHandleBase(const JoinHandleBase&) = delete;
  JoinHandleBase(JoinHandleBase&& rhs) noexcept
    : rt(rhs.rt)
    , m_state(rhs.m_state.load()) {};
  ~JoinHandleBase() = default;

  auto operator=(const JoinHandleBase&) -> JoinHandleBase& = delete;
  auto operator=(JoinHandleBase&&) -> JoinHandleBase& = delete;

  auto await_ready() -> bool;
  void await_suspend(std::coroutine_handle<>);
  auto get_task() -> Task const* { return m_state.load()->dependent; }
  void kill();

  /* actual specialization for the implementation occurs
   * in the template JoinHandle */
  template<typename Return>
  auto await_resume() -> Return
  {
    auto& state = *m_state.load();

    if (!ready)
      state.mutex.lock();

    if (not state.entry_coro.get_handle().done()) {
      fprintf(stderr,
              "fatal joinhandle error, resuming awaitable even though the "
              "coroutine is NOT finished\n"),
        std::terminate();
    }

    state.mutex.unlock();
    return std::move((static_cast<typename Coro<Return>::promise_type&>(
                        state.entry_coro.get_handle().promise()))
                       .retval.value());
  }

private:
  Runtime* rt;
  bool ready{ false };
  std::atomic<std::shared_ptr<SharedTaskState>> m_state;
};

/* ugh i can clean this up later.
 * for now, DONT TOUCH ANYTHING */
template<typename T>
class JoinHandle : public JoinHandleBase
{
public:
  using JoinHandleBase::JoinHandleBase;
  auto await_resume() -> T
  {
    return JoinHandleBase::await_resume<JoinHandle<T>, T>();
  }
};

};
