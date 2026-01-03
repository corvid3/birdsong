#include <optional>
#include <thread>

#include "common.hh"
#include "coro.hh"
#include "priv_runtime.hh"
#include "scheduler.hh"
#include "task.hh"
#include "tools/sleep.hh"

using namespace birdsong;

Sleep::Sleep(unsigned ms)
  : waker_ptr(new MutexWrapper(std::optional<Waker>(std::nullopt)))
{
  /* just spawn an OS thread. i'm lazy. sleep awaits aren't
   * too common anyways, so the cost is minimal.
   */
  std::thread(
    [ptr = this->waker_ptr](unsigned ms) mutable {
      std::this_thread::sleep_for(std::chrono::milliseconds(ms));
      ptr->with_lock([](auto& in) mutable {
        if (in)
          in->wake();
      });
    },
    ms)
    .detach();
};

void
Sleep::await_suspend(std::coroutine_handle<> handle)
{
  Runtime& rt = basic_handle_from_void(handle).promise().runtime;
  Waker waker = rt.create_waker();
  this->waker_ptr->with_lock(
    [&](auto& in) mutable { in.emplace(std::move(waker)); });
}
