#pragma once

#include "../common.hh"
#include "../task.hh"

#include <condition_variable>
#include <thread>

namespace birdsong {

/* starts the sleep thread immediately upon construction */
class Sleep : public AwaitableBase
{
public:
  Sleep(const Sleep&) = delete;
  Sleep(Sleep&&) = delete;
  auto operator=(const Sleep&) -> Sleep& = delete;
  auto operator=(Sleep&&) -> Sleep& = delete;

  explicit Sleep(unsigned ms);
  ~Sleep();

  void reset(unsigned ms);
  auto await_suspend(std::coroutine_handle<>) -> bool;

private:
  struct Data
  {
    struct Data2
    {
      std::optional<Waker> waker;
      bool done{ false };
    };

    MutexWrapper<Data2> waker;
    std::mutex flag_mutex;
    std::condition_variable flag;
    std::atomic<unsigned> ms;
    bool deleting{ false };
  };

  std::thread sleep_thread;
  Data* data;
};

};
