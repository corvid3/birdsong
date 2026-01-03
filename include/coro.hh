#pragma once

#include <any>
#include <concepts>
#include <coroutine>
#include <iostream>
#include <optional>
#include <utility>

#include "common.hh"

namespace birdsong {

class Runtime;

/* represents the barest information required for a coroutine
 * to resume/start/end. return type information is provided
 * in the Coro derived class. */
class PromiseBase
{
public:
  PromiseBase(Runtime& runtime)
    : handle(std::coroutine_handle<PromiseBase>::from_promise(*this))
    , runtime(runtime) {};

  static std::coroutine_handle<PromiseBase> handle_from_void(
    std::coroutine_handle<> const& handle);

  ~PromiseBase() = default;
  void unhandled_exception();
  std::suspend_always initial_suspend();

  struct final_kill
  {
    bool await_ready() noexcept;
    void await_suspend(std::coroutine_handle<>) noexcept;
    void await_resume() noexcept;
  };

  final_kill final_suspend() noexcept;

  std::coroutine_handle<PromiseBase> handle;
  Runtime& runtime;

  /* parent coroutine promise */
  PromiseBase* parent = nullptr;
};

using BasicHandle = std::coroutine_handle<PromiseBase>;

BasicHandle inline basic_handle_from_void(std::coroutine_handle<> handle)
{
  return BasicHandle::from_address(handle.address());
}

/* base class so that the runtime can cast down a templated coroutine
 * to this and then grab the base promise type */
class CoroBase
{
public:
  CoroBase(PromiseBase& promise)
    : m_promise(promise) {};
  ~CoroBase();

  PromiseBase& get_promise() { return m_promise; }

protected:
  PromiseBase& m_promise;
};

/* stupid jank to hide a method */
class CoroAwaiterImpl
{
  struct AwaiterImpl
  {
  public:
    /* handles the suspension code for
     * managing the call stack in the scheduler */
    void update_task_suspend(BasicHandle inside, BasicHandle outside);

    /* handles the resumation code for managing
     * the call stack in the scheduler */
    void update_task_resume(BasicHandle inside, BasicHandle outside);
  };

  template<typename T>
  friend class Coro;
};

template<typename T = Empty>
class Coro : public CoroBase
{
public:
  struct promise_type : PromiseBase
  {
    promise_type(Runtime& runtime, auto const&...)
      : PromiseBase(runtime) {};

    /* workaround for non-static lambdas (ignores this argument) */
    promise_type(auto const&, Runtime& runtime, auto const&...)
      : PromiseBase(runtime) {};

    Coro get_return_object() & { return Coro(*this); }
    void return_value(T&& in) { retval.emplace(std::move(in)); }
    std::optional<T> retval;
  };

  /* Coro awaits act as cooperative scheduling points in the runtime,
   * after an await is scheduled, we pause the current task
   */
  struct Awaiter
    : AwaitableBase
    , CoroAwaiterImpl::AwaiterImpl
  {
    Awaiter(std::coroutine_handle<PromiseBase> inside)
      : inside(inside) {};

    void await_suspend(std::coroutine_handle<> outside)
    {
      this->outside = basic_handle_from_void(outside);

      /* update the tasks in the scheduler to manage the call chain */
      update_task_suspend(inside, this->outside);
    }

    T await_resume()
    {
      update_task_resume(inside, outside);
      /* update the tasks in the scheduler to manage the call chain */
      promise_type& promise = static_cast<promise_type&>(inside.promise());

      if (!promise.retval.has_value())
        std::cerr << ("promise is NOT fulfilled despite coroutine awaiter "
                      "resuming! panicking!"),
          std::terminate();

      return std::move(*promise.retval);
    }

  private:
    std::coroutine_handle<PromiseBase> inside;
    std::coroutine_handle<PromiseBase> outside{};
  };

  Coro(promise_type& promise)
    : CoroBase(promise) {};

  Awaiter operator co_await() const noexcept
  {
    return Awaiter(std::coroutine_handle<PromiseBase>::from_promise(m_promise));
  }

  std::optional<T>& poll() { return get_concretized_handle().promise().future; }

private:
  std::coroutine_handle<promise_type> get_concretized_handle()
  {
    return std::coroutine_handle<promise_type>::from_promise(
      dynamic_cast<promise_type&>(m_promise));
  }
};

template<typename T, typename R>
concept ConvertibleToCoro = requires(T t) {
  { t(std::declval<Runtime&>()) } -> std::same_as<Coro<R>>;
};

};
