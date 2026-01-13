#pragma once

#include <coroutine>
#include <memory>

#include "../common.hh"
#include "../runtime.hh"
#include "../task.hh"

namespace birdsong {

/* awaitable cancellation token that immediately
 * wakes all awaiting tasks when set.
 * WARNING: any tasks waiiting on a token that is never set
 *    will never be finished or killed.
 * TODO: maybe make TokenOwner & TokenHandles and then
 *       automatically set a token when the Owner is dropped?
 */
class Token : public AwaitableBase
{
  struct Impl;

public:
  auto static constexpr SelectCoro = [](Runtime&, Empty) static -> Coro<> {
    co_return {};
  };

  Token();
  ~Token();

  operator bool() const;

  bool await_ready();
  bool await_suspend(std::coroutine_handle<>);

  void go();

private:
  std::shared_ptr<Impl> m_impl;
  std::optional<std::list<Waker>::iterator> m_waker;
};

};
