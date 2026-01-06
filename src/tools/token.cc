#include <coroutine>
#include <list>

#include "common.hh"
#include "coro.hh"
#include "priv_runtime.hh"
#include "task.hh"
#include "tools/token.hh"

using namespace birdsong;

struct Token::Impl
{
  struct Data
  {
    bool flag;
    std::list<Waker> waiting;
  };

  MutexWrapper<Data> data;
};

Token::Token()
  : m_impl(new Impl) {};

Token::
operator bool() const
{
  return m_impl->data.with_lock([](auto const& in) { return in.flag; });
};

bool
Token::await_ready()
{
  return false;
}

bool
Token::await_suspend(std::coroutine_handle<> handle)
{
  return m_impl->data.with_lock([&](Impl::Data& in) {
    if (in.flag)
      return false;

    in.waiting.push_back(
      basic_handle_from_void(handle).promise().runtime.create_waker());

    this->m_waker = --in.waiting.end();

    return true;
  });
}

void
Token::go()
{
  m_impl->data.with_lock([&](Impl::Data& data) {
    data.flag = true;
    for (auto& waiter : data.waiting)
      waiter.wake();
  });
}

Token::~Token()
{
  m_impl->data.with_lock([&](Impl::Data& in) {
    if (this->m_waker) {
      (*this->m_waker)->wake();
      in.waiting.erase(*this->m_waker);
    }
  });
}
