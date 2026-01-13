#include <coroutine>
#include <cstddef>
#include <cstdio>
#include <exception>
#include <functional>
#include <print>
#include <stdexcept>
#include <task.hh>
#include <utility>

#include "atomic.hh"
#include "coro.hh"
#include "io.hh"
#include "reactor.hh"
#include "runtime.hh"
#include "tools/channel.hh"
#include "tools/select.hh"
#include "tools/sleep.hh"
#include "tools/tcp.hh"
#include "tools/token.hh"

using namespace birdsong;

Token token;

int
main()
{

  std::thread thread([&]() {
    Runtime(std::unique_ptr<Reactor>(new PollReactor)).run([&]() -> Coro<> {
      printf("test\n");
      co_await Select(
        SelectCase(Sleep(500), [](Empty) -> Coro<> { co_return {}; }));
      printf("test\n");
      token.go();
      printf("test\n");

      co_return {};
    });
  });

  // getchar();
  // tok.go();
  thread.join();
}

/* *** */
// Runtime(4).run([](Runtime& rt) -> Coro<> {
//   Token token;

//   auto [tx, rx] = Channel<TCPSocket>::Create();

//   auto acceptor_task = rt.spawn_lambda(
//     [&, token, tx = std::move(tx)](Runtime& rt) mutable -> Coro<> {
//       TCPListener listener(6969, { true });

//       while (not token) {
//         co_await Select(
//           rt,
//           std::pair{ token, Token::SelectCoro },
//           std::pair{
//             listener.accept(),
//             [&tx](Runtime& rt,
//                   std::optional<TCPSocket> socket) mutable -> Coro<> {
//               if (!socket)
//                 co_return {};

//               tx.send(std::move(*socket));

//               co_return {};
//             },
//           });
//       }

//       co_return {};
//     });

//   auto con_handler = [&rt, &token](Runtime&, TCPSocket socket) -> Coro<> {
//     std::array<std::byte, 512> buf;
//     bool disconnect{ false };

//     while (not token and not disconnect) {
//       co_await Select(
//         rt,
//         std::pair{ token, Token::SelectCoro },
//         std::pair{
//           socket.read(buf),
//           [&](Runtime&, std::expected<unsigned, unsigned> val) -> Coro<> {
//             if (val.has_value()) {
//               if (val.value() == 0)
//                 disconnect = true;
//               else
//                 co_await socket.write(buf);
//             } else
//               std::cerr << "socket error\n", disconnect = true;

//             co_return {};
//           },
//         });
//     }
//     co_return {};
//   };

//   auto sleep_task = Sleep(100);

//   while (not token) {
//     co_await Select(rt,
//                     std::pair{
//                       rx,
//                       [&](Runtime&, TCPSocket socket) -> Coro<> {
//                         rt.spawn(con_handler(rt, std::move(socket)));
//                         co_return {};
//                       },
//                     },
//                     std::pair{
//                       sleep_task,
//                       [&](Runtime&, Empty) -> Coro<> {
//                         printf("went\n");
//                         token.go();
//                         co_return {};
//                       },
//                     });
//   }

//   /* join */
//   co_await acceptor_task;

//   co_return {};
// });

/* *** */

// struct empty_awaitable
// {
//   bool await_ready() { return false; }
//   void await_suspend(std::coroutine_handle<> handle)
//   {
//     waker.reset(new Waker(
//       basic_handle_from_void(handle).promise().runtime.create_waker()));
//   }

//   Empty await_resume() { return {}; }

//   std::unique_ptr<Waker> waker;
// };

// Token tok;
// Runtime(4).run([&tok](Runtime& rt) -> Coro<> {
//   for (unsigned i = 0; i < 32; i++)
//     rt.spawn_lambda([&tok](Runtime& rt) -> Coro<> {
//       co_await Select(
//         rt,
//         std::pair{ tok, Token::SelectCoro },
//         std::pair{ empty_awaitable(),
//                    [](Runtime&, Empty) -> Coro<> { co_return {}; } });

//       co_return {};
//     });

//   tok.go();
//   co_return {};
// });
