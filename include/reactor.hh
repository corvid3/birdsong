#pragma once

#include <memory>

#include "atomic.hh"
#include "task.hh"

namespace birdsong {

/* heart of the asynchronous IO implementation
 * reactor implementations read across a set of
 * inserted filedescriptor-waker-listenmask tuples,
 * and when the file-descriptor eventually has events
 * that are within the listenmask, the waker associated
 * is invoked. */
class Reactor : public Atom
{
public:
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

  virtual ~Reactor() = default;

  virtual void insert(FDWait) = 0;
  virtual void poll() = 0;
};

class PollReactor : public Reactor
{
  /* TODO: every once in a while check if any of the
   * inserted wakers/tasks have been killed.
   * if they have been killed, just remove the waker */

public:
  struct Data;

  PollReactor();
  ~PollReactor();

  void insert(FDWait) override;
  void poll() override;

  Data& get_data(Atom::Key) { return *m_data; }

private:
  std::unique_ptr<Data> m_data;
};

};
