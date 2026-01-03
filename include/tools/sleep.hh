#pragma once

#include "../common.hh"

namespace birdsong {

class Sleep : public AwaitableBase
{
public:
  Sleep(unsigned ms);

  void await_suspend(std::coroutine_handle<>);

private:
  unsigned ms;
};

};
