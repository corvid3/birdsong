#pragma once

#include <deque>
#include <list>
#include <queue>

#include "../common.hh"
#include "../runtime.hh"
#include "../task.hh"

namespace birdsong {

template<typename T, typename Container = std::deque<T>>
class Channel
{
public:
  class Receive : public AwaitableBase
  {
  public:
    Receive(std::shared_ptr<Channel> channel)
      : m_channel(channel) {};

    Receive(const Receive&) = default;
    Receive(Receive&& from) { this->m_channel = std::move(from.m_channel); };

    Receive& operator=(const Receive&) = default;
    Receive& operator=(Receive&&) = default;

    bool await_ready() const&
    {
      m_channel->mutex.lock();
      return instant = not m_channel->m_queue.empty();
    }

    void await_suspend(std::coroutine_handle<> handle) const&
    {
      PromiseBase& promise = basic_handle_from_void(handle).promise();
      Runtime& rt = promise.runtime;
      m_channel->m_recvWaker.emplace(rt.create_waker());
      m_channel->mutex.unlock();
    }

    T await_resume() const&
    {
      if (not instant)
        m_channel->mutex.lock();

      T out = std::move(m_channel->m_queue.front());
      m_channel->m_queue.pop();
      m_channel->mutex.unlock();
      return out;
    }

  private:
    std::shared_ptr<Channel> m_channel;

    /* due to how task waking works, the lock must be
     * carried over from await_ready to await_suspend.
     * however, if ready immediately returns true,
     * the lock will continue to be locked and immediately
     * skip to await_resume. therefore, we store the state
     * of the await_ready return value and optionally
     * lock if it failed to immediately return */
    mutable bool instant;
  };

  class Send
  {
  public:
    Send(std::shared_ptr<Channel> channel)
      : m_channel(channel) {};

    Send(const Send&) = default;
    Send(Send&&) = default;
    Send& operator=(const Send&) = default;
    Send& operator=(Send&&) = default;

    void send(T&& in)
    {
      m_channel->mutex.lock();
      m_channel->m_queue.emplace(std::move(in));

      if (m_channel->m_recvWaker) {
        m_channel->m_recvWaker->wake();
        m_channel->m_recvWaker.reset();
      }

      m_channel->mutex.unlock();
    }

  private:
    std::shared_ptr<Channel> m_channel;
  };

  static std::pair<Send, Receive> Create()
  {
    std::shared_ptr<Channel> const chan(new Channel());
    return { Send(chan), Receive(chan) };
  }

private:
  Channel() = default;

  Mutex mutex;
  std::queue<T, Container> m_queue{};
  std::optional<Waker> m_recvWaker;
};

};
