#include <memory>
#include <utility>

#include "priv_runtime.hh"
#include "runtime.hh"
#include "task.hh"

using namespace birdsong;

Waker::Waker(Runtime& runtime, std::unique_ptr<Task> task)
  : runtime(runtime)
  , task(std::move(task)) {};

Waker::Waker(Waker&& rhs)
  : runtime(rhs.runtime)
  , task(std::move(*rhs.acquire())) {

  };

void
Waker::wake()
{
  auto transaction = acquire();

  /* if the task no longer exists...
   * just quit. */
  if (not task)
    return;

  runtime.m_threadQueue.push_task(
    [&runtime = this->runtime, task = std::move(this->task)]() mutable {
      auto state = task->acquire()->state.load();
      state->mutex.lock();
      auto valid = not state->killswitch;

      auto handle = (runtime.acquire()
                       ->m_threadData.at(ThreadQueue::GetThisThreadID())
                       .m_currentTask = std::move(task))
                      ->acquire()
                      ->handle;

      if (valid && handle)
        handle.resume();

      auto fin = std::move(runtime.acquire()
                             ->m_threadData.at(ThreadQueue::GetThisThreadID())
                             .m_currentTask);

      state->mutex.unlock();
    });
}

static std::atomic<int> m{ 0 };

Task::Task(Runtime& rt, Coro<> coro)
  : m_data{ coro.get_handle(),
            std::make_shared<SharedTaskState>(*this, std::move(coro)) }
{
  tag = m++;
  rt.acquire()->m_aliveTasks++;
};

void
Task::kill()
{
  auto transaction = acquire();
  transaction->state.load()->killswitch = true;
  Runtime& rt = transaction->handle.promise().runtime;

  // if (transaction->handle) {
  //   auto handle = transaction->handle;
  //   transaction->handle = nullptr;
  //   // if (not handle.done())
  //   //   handle.destroy();
  // }

  rt.acquire()->m_aliveTasks--;
}

Task::~Task()
{
  acquire()->state.load()->mutex.lock();
  if (not acquire()->state.load()->killswitch)
    kill();

  for (auto& join_handles : acquire()->state.load()->join_handle_wakers)
    join_handles.wake();
  acquire()->state.load()->mutex.unlock();
};

bool
JoinHandleBase::await_ready()
{
  m_state.load()->mutex.lock();
  ready = m_state.load()->killswitch;
  return ready;
}

void
JoinHandleBase::await_suspend(std::coroutine_handle<>)
{
  m_state.load()->join_handle_wakers.emplace_back(rt.create_waker());
  m_state.load()->mutex.unlock();
}

void
JoinHandleBase::kill()
{
  m_state.load()->mutex.lock();
  if (not m_state.load()->killswitch)
    m_state.load()->dependent.kill();
  m_state.load()->mutex.unlock();

  m_state.load().reset();
}
