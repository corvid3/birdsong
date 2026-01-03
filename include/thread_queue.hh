#pragma once

#include <condition_variable>
#include <functional>
#include <queue>
#include <thread>
#include <vector>

namespace birdsong {

/* basic thread/worker queue used for parallel computations */
class ThreadQueue
{
  class Worker;

public:
  using ThreadID = unsigned;

  /* each thread contains a ThreadQueue specific unique ID
   * associated with said thread. the ThreadID of the thread
   * chosen to execute a given job will be passed as the
   * only argument of the Job. */
  using Job = std::move_only_function<void()>;

  constexpr static unsigned MainThread = -1u;
  static unsigned GetThisThreadID();

  ThreadQueue(unsigned num_workers = std::thread::hardware_concurrency());
  ~ThreadQueue();

  ThreadQueue(const ThreadQueue&) = delete;
  ThreadQueue(ThreadQueue&&) = delete;
  ThreadQueue& operator=(const ThreadQueue&) = delete;
  ThreadQueue& operator=(ThreadQueue&&) = delete;

  /* simple operator wrapper around push_task */
  ThreadQueue& operator+=(Job&&);
  void push_task(Job&&);

  bool quitting() const { return m_taskQueueQuit; }

private:
  std::condition_variable m_taskQueueNotify;
  std::mutex m_taskQueueMutex;
  std::queue<Job*> m_taskQueue;
  std::atomic<int> m_numWorking;
  std::vector<std::pair<std::thread, Worker*>> m_threads;

  /* if true, the next time taskQueueNotify is triggered
   * the accepting thread will return & await to be joined */
  bool m_taskQueueQuit{ false };
};

};
