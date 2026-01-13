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
  std::exception_ptr exception = nullptr;
  Runtime* runtime = nullptr;

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
  CoroBase(BasicHandle handle)
    : m_inside(handle) {};
  ~CoroBase();

  CoroBase(const CoroBase&) = delete;
  CoroBase& operator=(const CoroBase&) = delete;
  CoroBase(CoroBase&& rhs) noexcept
    : m_inside(rhs.m_inside)
    , m_outside(rhs.m_outside)
  {
    rhs.m_inside = nullptr;
    rhs.m_outside = nullptr;
  };

  CoroBase& operator=(CoroBase&&) = delete;

  BasicHandle get_handle() { return m_inside; }

protected:
  /* handles the suspension code for
   * managing the call stack in the scheduler */
  void update_task_suspend(BasicHandle inside, BasicHandle outside);

  /* handles the resumation code for managing
   * the call stack in the scheduler */
  void update_task_resume(BasicHandle inside, BasicHandle outside);

  BasicHandle m_inside;
  BasicHandle m_outside = nullptr;
};

template<typename T = Empty>
class Coro : public CoroBase
{
public:
  Coro(BasicHandle handle)
    : CoroBase(handle)
  {
    assert(m_inside);
  };

  Coro(const Coro&) = delete;
  Coro& operator=(const Coro&) = delete;
  Coro(Coro&&) = default;
  Coro& operator=(Coro&&) = delete;

  struct promise_type : PromiseBase
  {
    Coro get_return_object() &
    {
      return Coro(BasicHandle::from_promise(*this));
    }

    void return_value(T&& in) { retval.emplace(std::move(in)); }
    std::optional<T> retval;
  };

  bool await_ready() { return false; }

  /* Coro awaits act as cooperative scheduling points in the runtime,
   * after an await is scheduled, we pause the current task
   */
  void await_suspend(std::coroutine_handle<> outside)
  {
    m_outside = basic_handle_from_void(outside);

    /* update the tasks in the scheduler to manage the call chain */
    update_task_suspend(m_inside, m_outside);
  }

  T await_resume()
  {
    update_task_resume(m_inside, m_outside);
    /* update the tasks in the scheduler to manage the call chain */
    promise_type& promise = static_cast<promise_type&>(m_inside.promise());

    if (!promise.retval.has_value())
      std::cerr << ("promise is NOT fulfilled despite coroutine awaiter "
                    "resuming! panicking!"),
        std::terminate();

    return std::move(*promise.retval);
  }
};

template<typename T, typename R>
concept ConvertibleToCoro = requires(T t) {
  { t(std::declval<Runtime&>()) } -> std::same_as<Coro<R>>;
};

};
