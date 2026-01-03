#pragma once

#include <coroutine>
#include <memory>

#include "../common.hh"
#include "../scheduler.hh"

namespace birdsong {

/* awaitable cancellation token that immediately
 * wakes all awaiting tasks when set */
class Token : public AwaitableBase
{
  struct Impl;

public:
  auto static constexpr SelectCoro = [](Runtime&, Empty) -> Coro<> {
    co_return {};
  };

  Token();

  operator bool() const;

  bool await_ready();
  void await_suspend(std::coroutine_handle<>);
  void go();

private:
  std::shared_ptr<Impl> m_impl;
};

};
