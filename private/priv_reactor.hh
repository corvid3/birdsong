#pragma once

#include <memory>

#include "atomic.hh"
#include "task.hh"

namespace birdsong {

class Reactor : public Atom
{
  /* TODO: every once in a while check if any of the
   * inserted wakers/tasks have been killed.
   * if they have been killed, just remove the waker */

public:
  struct Data;

  struct WaitMask
  {
    bool read;
    bool write;
  };

  struct FDWait
  {
    Waker waker;
    unsigned fd;
    WaitMask mask;
  };

  struct Result
  {
    Waker waker;
    WaitMask mask;
  };

  Reactor();
  ~Reactor();

  void insert(FDWait);
  bool anything_to_poll();

  /* if the scheduler has no tasks to execute
   * and only things to poll, then we allow the
   * poller to block the thread for some milliseconds
   * to reduce CPU usage */
  void poll(bool blocking_allowed);

  Data& get_data(Atom::Key) { return *m_data; }

private:
  std::unique_ptr<Data> m_data;
};

};
