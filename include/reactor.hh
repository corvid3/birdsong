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
  Reactor() = default;
  Reactor(const Reactor&) = delete;
  Reactor(Reactor&&) = delete;
  auto operator=(const Reactor&) -> Reactor& = delete;
  auto operator=(Reactor&&) -> Reactor& = delete;
  virtual ~Reactor() = default;

  struct WaitMask
  {
    bool read;
    bool write;
  };

  struct FDWait
  {
    Waker waker;
    signed fd;
    WaitMask mask;
  };

  struct Result
  {
    Waker waker;
    WaitMask mask;
  };

  virtual void insert(FDWait) = 0;
  virtual void poll() = 0;
};

class PollReactor final : public Reactor
{
  /* TODO: every once in a while check if any of the
   * inserted wakers/tasks have been killed.
   * if they have been killed, just remove the waker */

public:
  struct Data;

  PollReactor();
  ~PollReactor() final;

  PollReactor(const PollReactor&) = delete;
  PollReactor(PollReactor&&) = delete;
  auto operator=(const PollReactor&) -> PollReactor& = delete;
  auto operator=(PollReactor&&) -> PollReactor& = delete;

  void insert(FDWait /*unused*/) override;
  void poll() override;

  auto get_data(Atom::Key /*unused*/) -> Data& { return *m_data; }

private:
  std::unique_ptr<Data> m_data;
};

};
