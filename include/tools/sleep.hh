#pragma once

#include "../common.hh"
#include "../task.hh"
#include <memory>

namespace birdsong {

/* starts the sleep thread immediately upon construction */
class Sleep : public AwaitableBase
{
public:
  Sleep(unsigned ms);

  void await_suspend(std::coroutine_handle<>);

private:
  std::shared_ptr<MutexWrapper<std::optional<Waker>>> waker_ptr;
};

};
