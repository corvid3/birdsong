#include <atomic>
#include <condition_variable>
#include <thread>

#include "common.hh"
#include "coro.hh"
#include "runtime.hh"
#include "task.hh"
#include "tools/sleep.hh"

using namespace birdsong;

Sleep::Sleep(unsigned ms)
{
  std::unique_ptr<Data> ptr(new Data);
  data = ptr.get();
  data->ms = ms;

  /* just spawn an OS thread. i'm lazy. sleep awaits aren't
   * too common anyways, so the cost is minimal.
   */
  sleep_thread = std::thread(
    [](std::unique_ptr<Data> data) {
      while (true) {
        std::unique_lock lock(data->flag_mutex);

        if (data->deleting)
          break;

        /* if the flag was triggered instead of running out of time,
         * continue the loop */
        if (data->flag.wait_for(lock,
                                std::chrono::milliseconds(
                                  data->ms.load(std::memory_order::acquire))) ==
            std::cv_status::no_timeout)
          continue;

        data->waker.with_lock([](Data::Data2& in) {
          in.done = true;
          if (in.waker)
            in.waker->wake();
        });

        /* when we time out, wait on the flag again for when
         * the Sleep is .reset() or Data gets its destructor called */
        data->flag.wait(lock);
      }
    },
    std::move(ptr));
};

Sleep::~Sleep()
{
  data->deleting = true;
  data->flag.notify_all();
  sleep_thread.join();
}

bool
Sleep::await_suspend(std::coroutine_handle<> handle)
{
  return this->data->waker.with_lock([&](Data::Data2& in) {
    Runtime& rt = basic_handle_from_void(handle).promise().runtime;
    Waker waker = rt.create_waker();
    if (in.done)
      return false;
    else
      in.waker.emplace(std::move(waker));
    return true;
  });
}

void
Sleep::reset(unsigned ms)
{
  this->data->ms = ms;
  this->data->flag.notify_one();
}
