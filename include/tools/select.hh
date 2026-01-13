#pragma once

#include <concepts>
#include <coroutine>
#include <functional>
#include <list>
#include <utility>

#include "../common.hh"
#include "../runtime.hh"
#include "../task.hh"

namespace birdsong {

template<typename T, typename L>
auto
SelectCase(T&& in, L const& lambda)
{
  return std::pair<decltype(std::forward<T>(in)), L>(std::forward<T>(in),
                                                     lambda);
}
template<typename... Cases>
Coro<Empty>
Select(Runtime& rt, Cases&&... cases)
{
  auto waker = rt.create_waker();
  Mutex finishMutex, listMutex;
  std::list<JoinHandleBase> join_handles;

  listMutex.lock();
  (
    [&](Cases&& in) {
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
    }(std::forward<Cases>(cases)),
    ...);

  listMutex.unlock();
  co_await std::suspend_always{};

  co_return {};
}

/* old implementation freezed in time until i can
 * figure out more of the perfect forwarding laws... */

// template<typename... Awaits, typename... Lambdas>
// Coro<Empty>
// Select(Runtime& rt, std::pair<Awaits, Lambdas>... cases)
// {
//   auto waker = rt.create_waker();
//   Mutex finishMutex, listMutex;
//   std::list<JoinHandleBase> join_handles;

//   listMutex.lock();
//   (
//     [&]<typename Await, typename Lambda>
//       requires std::convertible_to<
//                  Lambda,
//                  std::function<Coro<>(
//                    Runtime&,
//                    decltype(std::declval<Await>().await_resume()))>>
//     (std::pair<Await, Lambda> in) {
//       auto handle = rt.spawn_lambda([&](Runtime& rt) mutable -> Coro<> {
//         auto out = co_await in.first;

//         /* whoevers first to the finish line wins! */
//         if (!finishMutex.try_lock()) {
//           listMutex.lock();
//           for (auto& handle : join_handles)
//             if (handle.get_task() != rt.current_task())
//               handle.kill();
//           co_await in.second(rt, std::move(out));
//           waker.wake();
//         }

//         co_return {};
//       });

//       join_handles.emplace_back(std::move(handle));
//     }(std::move(cases)),
//     ...);

//   listMutex.unlock();
//   co_await std::suspend_always{};

//   co_return {};
// }

};
