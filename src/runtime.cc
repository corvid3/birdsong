#include <atomic>
#include <chrono>
#include <exception>
#include <memory>
#include <thread>

#include "priv_runtime.hh"
#include "reactor.hh"
#include "runtime.hh"
#include "task.hh"
#include "thread_queue.hh"

using namespace birdsong;

Runtime::Runtime(std::unique_ptr<Reactor> reactor, unsigned num_threads)
  : m_data(new Data)
  , m_reactor(std::move(reactor))
  , m_threadQueue(num_threads)
{
  for (unsigned i = 0; i < num_threads; i++) {
    acquire()->m_threadData[i] = {};
  }
};

Runtime::~Runtime() = default;

void
Runtime::run(std::function<Coro<>()> const& coro)
{
  if (acquire()->m_running) {
    std::cerr << "attempting to start multiple run loops on a single "
                 "crowroutine runtime\n",
      std::terminate();
  }

  acquire()->m_running = true;

  spawn_internal<Empty>(coro()).wake();

  while (acquire()->m_aliveTasks != 0) {
    get_reactor().poll();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}

/* unsets the current task & creates a waker set to it */
auto
Runtime::create_waker() -> Waker
{
  if (ThreadQueue::GetThisThreadID() == -1U) {
    std::cerr << "attempting to create a waker in a non-runtime thread, dont "
                 "call this method!",
      std::terminate();
  }

  auto& task = acquire()->get_this_thread_data().m_currentTask;

  if (!task)
    std::cerr << "no current task, panicking!\n", std::terminate();

  return { this, std::move(task) };
}

auto
Runtime::current_task() -> Task&
{
  return *acquire()->get_this_thread_data().m_currentTask;
}

auto
Runtime::num_tasks() -> unsigned
{
  return acquire()->m_aliveTasks;
}
