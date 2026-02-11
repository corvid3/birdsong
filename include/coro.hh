#pragma once

#include <cassert>
#include <concepts>
#include <coroutine>
#include <exception>
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
  PromiseBase()
    : handle(std::coroutine_handle<PromiseBase>::from_promise(*this)) {};

  PromiseBase(const PromiseBase&) = default;
  PromiseBase(PromiseBase&&) = delete;
  auto operator=(const PromiseBase&) -> PromiseBase& = default;
  auto operator=(PromiseBase&&) -> PromiseBase& = delete;

  static auto handle_from_void(std::coroutine_handle<> const& handle)
    -> std::coroutine_handle<PromiseBase>;

  ~PromiseBase() = default;
  void unhandled_exception();
  auto initial_suspend() -> std::suspend_always;

  struct final_kill
  {
    auto await_ready() noexcept -> bool;
    void await_suspend(std::coroutine_handle<>) noexcept;
    void await_resume() noexcept;
  };

  auto final_suspend() noexcept -> final_kill;

  std::coroutine_handle<PromiseBase> handle;
  std::exception_ptr exception = nullptr;
  Runtime* runtime = nullptr;

  /* parent coroutine promise */
  PromiseBase* parent = nullptr;
};

using BasicHandle = std::coroutine_handle<PromiseBase>;

auto inline basic_handle_from_void(std::coroutine_handle<> handle)
  -> BasicHandle
{
  return BasicHandle::from_address(handle.address());
}

/* base class so that the runtime can cast down a templated coroutine
 * to this and then grab the base promise type */
class CoroBase
{
public:
  explicit CoroBase(BasicHandle handle)
    : m_inside(handle) {};

  ~CoroBase();

  CoroBase(const CoroBase&) = delete;
  auto operator=(const CoroBase&) -> CoroBase& = delete;
  CoroBase(CoroBase&& rhs) noexcept
    : m_inside(rhs.m_inside)
    , m_outside(rhs.m_outside)
  {
    rhs.m_inside = nullptr;
    rhs.m_outside = nullptr;
  };

  auto operator=(CoroBase&& rhs) noexcept -> CoroBase&
  {
    this->~CoroBase();
    new (this) CoroBase(std::move(rhs));
    return *this;
  };

  auto get_handle() -> BasicHandle { return m_inside; }

  static auto await_ready() -> bool { return false; }

  /* Coro awaits act as cooperative scheduling points in the runtime,
   * after an await is scheduled, we pause the current task
   */
  void await_suspend(std::coroutine_handle<> outside)
  {
    m_outside = basic_handle_from_void(outside);

    /* update the tasks in the scheduler to manage the call chain */
    update_task_suspend(m_inside, m_outside);
  }

  template<typename T, typename promise_type>
  auto await_resume() -> T
  {
    update_task_resume(m_inside, m_outside);
    /* update the tasks in the scheduler to manage the call chain */
    auto& promise = static_cast<promise_type&>(m_inside.promise());

    if (!promise.return_value().has_value()) {
      std::cerr << ("promise is NOT fulfilled despite coroutine awaiter "
                    "resuming! panicking!"),
        std::terminate();
    }

    return std::move(*promise.return_value());
  }

protected:
  /* handles the suspension code for
   * managing the call stack in the scheduler */
  static void update_task_suspend(BasicHandle inside, BasicHandle outside);

  /* handles the resumation code for managing
   * the call stack in the scheduler */
  static void update_task_resume(BasicHandle inside, BasicHandle outside);

private:
  BasicHandle m_inside;
  BasicHandle m_outside = nullptr;
};

template<typename T = Empty>
class Coro : CoroBase
{
public:
  using result_type = T;

  using CoroBase::await_ready;
  using CoroBase::await_suspend;
  using CoroBase::get_handle;
  auto await_resume() -> T { return CoroBase::await_resume<T, promise_type>(); }

  explicit Coro(BasicHandle handle)
    : CoroBase(handle) {};

  explicit operator Coro<Empty>() { return *this; }

  ~Coro() = default;

  Coro(const Coro&) = delete;
  auto operator=(const Coro&) -> Coro& = delete;
  Coro(Coro&&) = default;
  auto operator=(Coro&&) -> Coro& = default;

  class promise_type : public PromiseBase
  {
  public:
    auto get_return_object() & -> Coro
    {
      return Coro(BasicHandle::from_promise(*this));
    }

    void return_value(T&& in) { retval.emplace(std::move(in)); }
    auto return_value() -> auto& { return retval; }

  private:
    std::optional<T> retval;
  };
};

template<typename T, typename R>
concept ConvertibleToCoro = requires(T t) {
  { t(std::declval<Runtime&>()) } -> std::same_as<Coro<R>>;
};

};
