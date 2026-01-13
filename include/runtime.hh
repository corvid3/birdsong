#pragma once

#include <functional>
#include <memory>
#include <utility>

#include "atomic.hh"
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
    /* ms between non-blocking poll invocations */
    unsigned poll_ms_wait = 10;
  };

  Runtime(std::unique_ptr<Reactor>, unsigned num_threads = 1);
  ~Runtime();

  /* thread-safe externally accessable data */
  struct Data;
  struct AtomicData;

  /* blockingly runs a coroutine
   * you should use this as your main entry point
   */
  void run(std::function<Coro<>(Runtime&)>);

  template<typename T>
  JoinHandle<T> spawn(Coro<T>&& coro)
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
    auto fnp = new decltype(std::function{ lambda })(std::move(lambda));

    return spawn([](Runtime& rt, auto* ptr) -> decltype(std::function{
                                              lambda })::result_type {
      auto&& val = co_await (*ptr)(rt);
      delete ptr;
      co_return std::move(val);
    }(*this, fnp));
  }

  // template<typename T>
  // auto spawn(std::function<Coro<T>(Runtime&)> auto const& fn)
  // {
  //   return spawn(std::function(fn));
  // }

  Data& get_data(Atom::Key) { return *m_data; };

  /* creates a waker from the current threads task
   * panics if called from outside of a runtimes thread */
  Waker create_waker();
  Task& current_task();
  unsigned num_tasks();

  Reactor& get_reactor() { return *m_reactor; }

private:
  template<typename T>
  Waker spawn_internal(CoroBase coro)
  {
    auto ptr =
      std::unique_ptr<Task>(new Task(*this, (Coro<>&&)std::move(coro)));
    return Waker(*this, std::move(ptr));
  }

  static void worker(Queue&);

  std::unique_ptr<Data> m_data;
  std::unique_ptr<Reactor> m_reactor;
  // std::unique_ptr<AtomicData> m_atomicData;
  ThreadQueue m_threadQueue;
};

};
