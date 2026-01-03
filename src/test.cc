#include <cstddef>
#include <cstdio>
#include <functional>
#include <task.hh>
#include <utility>

#include "atomic.hh"
#include "io.hh"
#include "scheduler.hh"
#include "tools/channel.hh"
#include "tools/select.hh"
#include "tools/sleep.hh"
#include "tools/tcp.hh"
#include "tools/token.hh"

using namespace birdsong;

int
main()
{
  Runtime(4).run([](Runtime& rt) -> Coro<> {
    Token token;

    auto [tx, rx] = Channel<TCPSocket>::Create();

    auto acceptor_task = rt.spawn_lambda(
      [&, token, tx = std::move(tx)](Runtime& rt) mutable -> Coro<> {
        TCPListener listener(6969, { true });

        while (not token) {
          co_await Select(
            rt,
            std::pair{ token, Token::SelectCoro },
            std::pair{
              listener.accept(),
              [&tx](Runtime& rt,
                    std::optional<TCPSocket> socket) mutable -> Coro<> {
                if (!socket)
                  co_return {};

                tx.send(std::move(*socket));

                co_return {};
              },
            });
        }

        co_return {};
      });

    auto con_handler = [&rt, &token](Runtime&, TCPSocket socket) -> Coro<> {
      std::array<std::byte, 512> buf;
      bool disconnect{ false };

      while (not token and not disconnect) {
        co_await Select(
          rt,
          std::pair{ token, Token::SelectCoro },
          std::pair{
            socket.recv(buf),
            [&](Runtime&, std::expected<unsigned, unsigned> val) -> Coro<> {
              if (val.has_value()) {
                if (val.value() == 0)
                  disconnect = true;
                else
                  co_await socket.send(buf);
              } else
                std::cerr << "socket error\n", disconnect = true;

              co_return {};
            },
          });
      }
      co_return {};
    };

    auto sleep_task = Sleep(100);

    while (not token) {
      co_await Select(rt,
                      std::pair{
                        rx,
                        [&](Runtime&, TCPSocket socket) -> Coro<> {
                          rt.spawn(con_handler(rt, std::move(socket)));
                          co_return {};
                        },
                      },
                      std::pair{
                        sleep_task,
                        [&](Runtime&, Empty) -> Coro<> {
                          token.go();
                          co_return {};
                        },
                      });
    }

    /* join */
    co_await acceptor_task;

    co_return {};
  });
}
