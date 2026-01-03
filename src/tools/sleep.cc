#include <thread>

#include "coro.hh"
#include "priv_runtime.hh"
#include "scheduler.hh"
#include "task.hh"
#include "tools/sleep.hh"

using namespace birdsong;

Sleep::Sleep(unsigned ms)
  : ms(ms) {};

void
Sleep::await_suspend(std::coroutine_handle<> handle)
{
  Runtime& rt = basic_handle_from_void(handle).promise().runtime;
  Waker waker = rt.create_waker();

  /* just spawn an OS thread. i'm lazy. sleep awaits aren't
   * too common anyways, so the cost is minimal.
   */
  std::thread(
    [waker = std::move(waker)](unsigned ms) mutable {
      std::this_thread::sleep_for(std::chrono::milliseconds(ms));
      waker.wake();
    },
    ms)
    .detach();
}
