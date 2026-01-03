#include "coro.hh"
#include <coroutine>
#include <exception>

#include "priv_runtime.hh"
#include "scheduler.hh"

using namespace birdsong;

bool
AwaitableBase::await_ready()
{
  return false;
}

void AwaitableBase::await_suspend(std::coroutine_handle<>){};

Empty
AwaitableBase::await_resume()
{
  return {};
};

void
CoroAwaiterImpl::AwaiterImpl::update_task_suspend(BasicHandle inside,
                                                  BasicHandle outside)
{
  /* grab the current task handle, and set its promise to be that of
   * the lower coro promise. this will cause the lower coro
   * to be executed when the task is executed next */
  auto& runtime = outside.promise().runtime;
  runtime.acquire()->get_this_thread_data().m_currentTask->acquire()->handle =
    inside;

  /* update the lower coro promise's parent to point to the upper promise */
  inside.promise().parent = &outside.promise();
  /* push the task back onto the queue */
  runtime.create_waker().wake();
}

void
CoroAwaiterImpl::AwaiterImpl::update_task_resume(BasicHandle inside,
                                                 BasicHandle outside)
{
  if (outside)
    inside.promise()
      .runtime.acquire()
      ->get_this_thread_data()
      .m_currentTask->acquire()
      ->handle = PromiseBase::handle_from_void(outside);
}

std::coroutine_handle<PromiseBase>
PromiseBase::handle_from_void(std::coroutine_handle<> const& handle)
{
  return std::coroutine_handle<PromiseBase>::from_address(handle.address());
}

void
PromiseBase::unhandled_exception()
try {
  std::rethrow_exception(std::current_exception());
} catch (std::exception const& e) {
  std::cerr << e.what() << std::endl;
  std::terminate();
}

std::suspend_always
PromiseBase::initial_suspend()
{
  return {};
}

bool
PromiseBase::final_kill::await_ready() noexcept
{
  return false;
}

void
PromiseBase::final_kill::await_suspend(std::coroutine_handle<> handle) noexcept
{
  PromiseBase& prom = basic_handle_from_void(handle).promise();

  if (prom.parent) {
    prom.runtime.current_task().acquire()->handle = prom.parent->handle;
    prom.runtime.create_waker().wake();
  }
}

void
PromiseBase::final_kill::await_resume() noexcept
{
  std::cerr << "invalid await_resume in final_kill\n";
  std::terminate();
}

auto
PromiseBase::final_suspend() noexcept -> final_kill
{
  return {};
}

CoroBase::~CoroBase()
{
  if (m_promise.handle && m_promise.handle.done())
    m_promise.handle.destroy();
}
