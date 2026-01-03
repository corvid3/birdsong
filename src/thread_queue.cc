#include "thread_queue.hh"
#include <iostream>
#include <mutex>

using namespace birdsong;

static thread_local unsigned threadID = -1u;

unsigned
ThreadQueue::GetThisThreadID()
{
  return threadID;
}

class ThreadQueue::Worker
{
public:
  Worker(ThreadQueue& jq)
    : m_jq(jq) {};

  void operator()(unsigned id)
  {
    threadID = id;

    for (;;) {
      std::unique_lock lock(m_jq.m_taskQueueMutex);

      m_jq.m_taskQueueNotify.wait(lock, [&] {
        return not m_jq.m_taskQueue.empty() or m_jq.m_taskQueueQuit;
      });

      /* quit exception */
      if (m_jq.m_taskQueueQuit)
        break;

      auto task = std::move(m_jq.m_taskQueue.front());
      m_jq.m_taskQueue.pop();

      lock.unlock();

      m_jq.m_numWorking++;
      (*task)();
      delete task;
      m_jq.m_numWorking--;
    }
  }

private:
  ThreadQueue& m_jq;
};

ThreadQueue::ThreadQueue(unsigned num_workers)
  : m_numWorking(0)
{
  for (unsigned i = 0; i < num_workers; i++) {
    Worker* worker = new Worker(*this);
    m_threads.push_back({ std::thread(*worker, i), worker });
  }
}

ThreadQueue::~ThreadQueue()
{
  m_taskQueueQuit = true;
  m_taskQueueNotify.notify_all();

  /* wait up to 5ms for any transactions to end before
   * killing every thread
   */
  for (unsigned attempts = 10; m_numWorking != 0 && attempts != 0; attempts--)
    std::this_thread::sleep_for(std::chrono::milliseconds(5));

  /* defer the worker data deletion until after terminating the threads */
  std::vector<Worker*> workers;
  for (auto& [thread, worker] : m_threads) {
    /* gracefully close any joinable threads */
    thread.join();

    workers.push_back(worker);
  }

  /* terminate every thread if they have not returned */
  m_threads.clear();

  /* delete the worker data */
  for (auto& worker : workers)
    delete worker;
}

ThreadQueue&
ThreadQueue::operator+=(Job&& task)
{
  push_task(std::move(task));
  return *this;
}

void
ThreadQueue::push_task(Job&& task)
{
  {
    std::lock_guard lock(m_taskQueueMutex);
    m_taskQueue.emplace(new Job(std::move(task)));
  }

  m_taskQueueNotify.notify_one();
}
