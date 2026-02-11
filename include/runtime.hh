#pragma once

#include <functional>
#include <memory>
#include <utility>

#include "atomic.hh"
#include "coro.hh"
#include "reactor.hh"
#include "task.hh"
#include "thread_queue.hh"

namespace birdsong {

class Runtime : public Atom
{
  /* theres cleaner ways to do this than have friend classes,
   * but i can clean this mess up later */
  friend class Task;
  friend class Waker;

  /* internal-only data */
  struct Queue;

public:
  struct Config
  {
    constexpr static auto default_poll_wait = 10;

    /* ms between non-blocking poll invocations */
    unsigned poll_ms_wait = default_poll_wait;
  };

  explicit Runtime(std::unique_ptr<Reactor>, unsigned num_threads = 1);
  ~Runtime();

  Runtime(const Runtime&) = delete;
  Runtime(Runtime&&) = delete;
  auto operator=(const Runtime&) -> Runtime& = delete;
  auto operator=(Runtime&&) -> Runtime& = delete;

  /* thread-safe externally accessable data */
  struct Data;
  struct AtomicData;

  /* blockingly runs a coroutine
   * you should use this as your main entry point
   */
  void run(std::function<Coro<>()> const&);

  template<typename T>
  auto spawn(Coro<T>&& coro) -> JoinHandle<T>
  {
    Waker waker = spawn_internal<T>(std::move(coro));
    auto out = JoinHandle<T>(*this, *waker.acquire()->get());
    waker.wake();
    return std::move(out);
  }

  auto spawn_lambda(auto const& lambda)
  {
    /* lambdas and coroutines are _insidious_
     * if you spawn a coroutine lambda without moving
     * the lambda/function to the heap, the spawner
     * may end up leaving scope. then, the lambda
     * ITSELF will be dropped from memory, causing
     * all captures to be dropped and cause UB.
     * so, move lambdas to the heap first and then
     * invoke them for the coroutine. */

    /* move it to the heap */
    auto fnp =
      std::make_unique<decltype(std::function{ lambda })>(std::move(lambda));

    return spawn(
      [](auto ptr) -> decltype(std::function{ lambda })::result_type {
        auto&& val = co_await (*ptr)();
        co_return std::move(val);
      }(std::move(fnp)));
  }

  auto get_data(Atom::Key /*unused*/) -> Data& { return *m_data; };

  /* creates a waker from the current threads task
   * panics if called from outside of a runtimes thread */
  auto create_waker() -> Waker;
  auto current_task() -> Task&;
  auto num_tasks() -> unsigned;
  auto get_reactor() -> Reactor& { return *m_reactor; }

private:
  template<typename T>
  auto spawn_internal(Coro<> coro) -> Waker
  {
    coro.get_handle().promise().runtime = this;

    /* evil downcast because it solves a problem */
    auto ptr = std::make_unique<Task>(*this, std::move(coro));

    return { this, std::move(ptr) };
  }

  static void worker(Queue&);

  std::unique_ptr<Data> m_data;
  std::unique_ptr<Reactor> m_reactor;
  ThreadQueue m_threadQueue;
};

class GetRuntime
{
public:
  static auto await_ready() -> bool { return false; }
  auto await_suspend(std::coroutine_handle<> handle) -> bool
  {
    rt = basic_handle_from_void(handle).promise().runtime;
    return false;
  }
  [[nodiscard]] auto await_resume() const -> Runtime* { return rt; }

private:
  Runtime* rt;
};

template<typename T>
class Spawn
{
public:
  explicit Spawn(Coro<T> co)
    : in(std::move(co)){};
  explicit Spawn(auto&& col)
  {
    /* lambdas and coroutines are _insidious_
     * if you spawn a coroutine lambda without moving
     * the lambda/function to the heap, the spawner
     * may end up leaving scope. then, the lambda
     * ITSELF will be dropped from memory, causing
     * all captures to be dropped and cause UB.
     * so, move lambdas to the heap first and then
     * invoke them for the coroutine. */

    /* move it to the heap */
    // auto fnp = new decltype(std::function{ col })(std::move(col));

    in.emplace([](auto fn) -> decltype(std::function{ col })::result_type {
      co_return co_await (fn)();
    }(std::forward(col)));
  };

  Spawn(const Spawn&) = delete;
  Spawn(Spawn&&) = delete;
  auto operator=(const Spawn&) -> Spawn& = delete;
  auto operator=(Spawn&&) -> Spawn& = delete;
  ~Spawn() = default;

  auto await_ready() -> bool { return false; }
  auto await_suspend(std::coroutine_handle<> handle) -> bool
  {
    rt = basic_handle_from_void(handle).promise().runtime;
    return false;
  }

  auto await_resume() -> JoinHandle<T> { return rt->spawn(std::move(*in)); }

private:
  Runtime* rt{};
  std::optional<Coro<T>> in;
};

template<typename T>
class LambdaDecomposer;
template<typename R, typename S>
struct LambdaDecomposer<R (S::*)() const>
{
  using Ret = R;
};
template<typename R, typename S>
struct LambdaDecomposer<R (S::*)()>
{
  using Ret = R;
};
template<typename T,
         typename sig = LambdaDecomposer<decltype(&T::
                                                  operator())>>
Spawn(T) -> Spawn<typename sig::Ret::result_type>;

};
