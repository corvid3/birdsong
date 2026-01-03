#include <memory>
#include <utility>

#include "priv_runtime.hh"
#include "scheduler.hh"
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

  auto task_trans = transaction->get()->acquire();

  /* for wakers with multiple consumers we need to check that the waker
   * has not yet been used */
  if (task_trans->state.load()->killswitch)
    return;

  runtime.m_threadQueue.push_task(
    [&runtime = this->runtime, task = std::move(this->task)]() mutable {
      auto handle = (runtime.acquire()
                       ->m_threadData.at(ThreadQueue::GetThisThreadID())
                       .m_currentTask = std::move(task))
                      ->acquire()
                      ->handle;

      handle.resume();

      auto fin = std::move(runtime.acquire()
                             ->m_threadData.at(ThreadQueue::GetThisThreadID())
                             .m_currentTask);
      fin.reset();
    });
}

static std::atomic<int> m{ 0 };

Task::Task(Runtime&, PromiseBase* promise)
  : m_data{ std::coroutine_handle<PromiseBase>::from_promise(*promise),
            std::make_shared<SharedTaskStateBase>(*this) }
{
  tag = m++;
  promise->runtime.get_atomic_data().m_aliveTasks++;
};

Task::~Task()
{
  auto transaction = acquire();

  transaction->handle.promise().runtime.get_atomic_data().m_aliveTasks--;
  transaction->handle.destroy();

  for (auto& join_handles : transaction->state.load()->join_handle_wakers)
    join_handles.wake();
};

void
JoinHandleBase::await_suspend(std::coroutine_handle<>)
{
  m_state.load()->join_handle_wakers.emplace_back(rt.create_waker());
  m_state.load()->mutex.unlock();
}

void
JoinHandleBase::kill()
{
  m_state.load()->killswitch = true;
}
