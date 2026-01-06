#include <cstring>
#include <optional>
#include <poll.h>
#include <sys/poll.h>
#include <sys/resource.h>
#include <vector>

#include "priv_reactor.hh"

using namespace birdsong;

struct Reactor::Data
{
  std::vector<pollfd> pollfds;
  std::vector<std::optional<FDWait>> wakers;
  unsigned max_pollfds{};

  unsigned laps = 0;

  /* used pollfd slot count
   * if < 1/2 of pollfds capacity & > 64, resize the
   * pollfd vector */
  unsigned used = 0;

  void remove_at(unsigned i)
  {
    wakers[i].reset();
    pollfds[i].fd = -1u;
    used--;
  }

  void insert_at(unsigned i, FDWait wait)
  {
    if (pollfds[i].fd != -1)
      std::terminate();

    pollfds[i].fd = wait.fd;
    pollfds[i].events = POLLHUP;
    pollfds[i].events |= (wait.mask.read ? POLLIN : 0);
    pollfds[i].events |= (wait.mask.write ? POLLOUT : 0);
    wakers[i].emplace(std::move(wait));
    used++;
  }

  /* returns an indice */
  unsigned allocate_pfd()
  {
    for (unsigned i = 0; pollfd& pfd : pollfds) {
      if (pfd.fd == -1)
        return i;

      i++;
    }

    unsigned prev_size = pollfds.size();
    unsigned new_size = std::min<unsigned>(prev_size * 1.5f, max_pollfds);
    pollfds.resize(new_size, pollfd{ -1, 0, 0 });
    wakers.resize(new_size);
    return prev_size;
  }

  Data()
    : pollfds(64, pollfd{ -1, 0, 0 })
    , wakers(64)
  {
    struct rlimit limit;

    /* if rlimit has a failure... theres a problem */
    if (getrlimit(RLIMIT_NOFILE, &limit) < 0)
      std::terminate();

    this->max_pollfds = limit.rlim_cur;
  };
};

Reactor::Reactor()
  : m_data(new Data) {};
Reactor::~Reactor() {}

bool
Reactor::anything_to_poll()
{
  return acquire()->used > 0;
}

void
Reactor::insert(FDWait wait)
{
  auto trans = acquire();
  trans->insert_at(trans->allocate_pfd(), std::move(wait));
}

void
Reactor::poll(bool)
{
  auto trans = acquire();
  auto num_updated = 0;

  if ((num_updated = ::poll(trans->pollfds.data(), trans->pollfds.size(), 0)) ==
      -1)
    throw std::runtime_error(std::format(
      "fatal poll error in reactor! {} {}", errno, strerror(errno)));

  /* every 50 "laps" of the reactor (up to 2 seconds)
   * iterate over all tasks in the reactor and check if
   * they're killed. if they are, drop them
   */
  if (trans->laps++ >= 50) {
    trans->laps = 0;

    for (unsigned i = 0; i < trans->pollfds.size(); i++) {
      auto& pfd = trans->pollfds[i];

      if (pfd.fd == -1)
        continue;

      auto& wait = trans->wakers[i];
      if (wait->waker.acquire()->get()->acquire()->state.load()->killswitch) {
        trans->wakers[i]->waker.wake();
        trans->remove_at(i);
      }
    }

    /* condensing logic */
    if (trans->used < trans->pollfds.size() / 2 &&
        trans->pollfds.size() / 2 > 64) {
      for (unsigned i = 0; i < trans->pollfds.size(); i++) {
        pollfd& pfd = trans->pollfds[i];
        std::optional<FDWait>& wait = trans->wakers[i];
        if (pfd.fd == -1) {
          for (unsigned j = i + 1; j < trans->pollfds.size(); j++) {
            pollfd& rhs = trans->pollfds[j];
            std::optional<FDWait>& rhswait = trans->wakers[j];
            if (rhs.fd != -1) {
              pfd = rhs;
              wait.emplace(std::move(*rhswait));
              rhswait.reset();
              rhs.fd = -1;
              break;
            }
          }
        }
      }

      /* still leave some slack above the low water mark */
      trans->pollfds.resize(trans->pollfds.size() * 0.7);
    }
  }

  /* only care to check the pollfds a second time
   * if there has been any updates */
  if (num_updated == 0)
    return;

  for (unsigned i = 0; i < trans->pollfds.size(); i++) {
    auto& pfd = trans->pollfds[i];

    if (pfd.fd == -1)
      continue;

    FDWait& meta = trans->wakers.at(i).value();

    if (pfd.revents != 0) {
      meta.waker.wake();
      trans->remove_at(i);
    }
  }
}
