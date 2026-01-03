#pragma once

#include <condition_variable>
#include <iostream>
#include <map>
#include <queue>

#include "priv_reactor.hh"
#include "scheduler.hh"
#include "task.hh"
#include "thread_queue.hh"

namespace birdsong {

/* non thread-safe data must go in here
 * so that the Atomic wrapper can do its work.
 * if you want to add some data that by itself is already thread safe,
 * add it to the AtomicData struct. */
struct Runtime::Data
{
  struct ThreadData
  {
    std::unique_ptr<Task> m_currentTask;
  };

  std::map<ThreadQueue::ThreadID, ThreadData> m_threadData;

  ThreadData& get_this_thread_data()
  {
    return m_threadData.at(ThreadQueue::GetThisThreadID());
  };
};

/* any kind of atomic-by-itself data goes in here,
 * so that the runtime doesn't have to be locked */
struct Runtime::AtomicData
{
  Reactor reactor;

  /* the scheduler will halt until a new task
   * is potentially added by another thread
   * if the task queue hits 0 size & there is nothing in the poller */
  std::condition_variable m_taskAlert;
  std::mutex m_taskAlertMutex;

  /* set to true in the run() method
   * calls std::terminate() if tries to run while another
   * run loop is currently active */
  std::atomic<bool> m_running;

  /* counter incremented/decremented by number of
   * tasks "alive" (not necessarily scheduled) in memory */
  std::atomic<unsigned> m_aliveTasks = 0;
};

}; // namespace birdsong
