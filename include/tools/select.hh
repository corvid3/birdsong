#pragma once

#include <concepts>
#include <coroutine>
#include <functional>
#include <list>

#include "../common.hh"
#include "../scheduler.hh"
#include "../task.hh"

namespace birdsong {

template<typename... Awaits, typename... Lambdas>
Coro<Empty>
Select(Runtime& rt, std::pair<Awaits, Lambdas>... cases)
{
  auto waker = rt.create_waker();
  Mutex finishMutex, listMutex;
  std::list<JoinHandleBase> join_handles;

  listMutex.lock();
  (
    [&]<typename Await, typename Lambda>
      requires std::convertible_to<
                 Lambda,
                 std::function<Coro<>(
                   Runtime&, decltype(std::declval<Await>().await_resume()))>>
    (std::pair<Await, Lambda>&& in) {
      auto handle = rt.spawn_lambda([&](Runtime& rt) mutable -> Coro<> {
        auto out = co_await in.first;

        /* whoevers first to the finish line wins! */
        if (!finishMutex.try_lock()) {
          listMutex.lock();
          for (auto& handle : join_handles)
            if (handle.get_task() != rt.current_task())
              handle.kill();
          co_await in.second(rt, std::move(out));
          waker.wake();
        }

        co_return {};
      });

      join_handles.emplace_back(std::move(handle));
    }(std::move(cases)),
    ...);

  listMutex.unlock();
  co_await std::suspend_always{};

  co_return {};
}

};
