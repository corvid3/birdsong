#pragma once

#include <concepts>
#include <cstdio>
#include <optional>

#include "common.hh"

namespace birdsong {

template<typename T, typename ACID>
concept Atomable = requires(T a) {
  typename T::Data;
  {
    a.get_data(typename ACID::Key{})
  } -> std::convertible_to<typename T::Data&>;
};

class Atom
{
public:
  class Key
  {
    friend class Atom;
    Key() = default;
  };

  template<typename Self>
  class Transaction
  {
    friend class Atom;
    Self* db;
    void copy(Self& into) { *into.acquire() = std::move(**this); }

  public:
    Transaction(const Transaction&) = delete;
    Transaction(Transaction&& rhs) noexcept
      : db(rhs.db)
    {
      rhs.db = nullptr;
    }

    Transaction& operator=(const Transaction&) = delete;
    Transaction& operator=(Transaction&& rhs)
    {
      return *new (this) Transaction(std::move(rhs));
    };

    Transaction(Self& db)
      : db(&db) {};

    typename Self::Data& operator*() const { return db->get_data(Key()); }
    typename Self::Data* operator->() const { return &db->get_data(Key()); }

    /* drops this transaction before destruction.
     * use this sparingly, it can result in some pretty unreasonable looking
     * code, but it is a necessary evil for certain patterns.
     */
    void drop()
    {
      if (db) {
        db->m_mutex.unlock();
        db = nullptr;
      }
    };

    ~Transaction()
    {
      if (db)
        db->m_mutex.unlock();
    };
  };

  Atom() = default;

  /* attempts to acquire the resource w/ blocking */
  template<typename Self>
    requires Atomable<std::decay_t<Self>, Atom>
  Transaction<Self> acquire(this Self& self)
  {
    self.m_mutex.lock();
    return Transaction<Self>(self);
  }

  /* attempts to acquire the resource w/o blocking
   * returns an optional */
  template<typename Self>
    requires Atomable<std::decay_t<Self>, Atom>
  std::optional<Transaction<Self>> try_acquire(this Self& self)
  {
    if (!self.m_mutex.try_lock())
      return std::nullopt;

    return std::optional(Transaction<Self>(self));
  }

private:
  Mutex mutable m_mutex;
};

/* TODO: async Atomable class */

/* simple acid wrapper around a type */
template<typename T>
class AtomWrapper : public Atom
{
public:
  AtomWrapper(T in)
    : m_data(std::move(in)) {};

  using Data = T;
  Data& get_data(Key) const { return m_data; };

private:
  Data mutable m_data;
};

};
