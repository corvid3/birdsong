#pragma once

#include <array>
#include <cstddef>
namespace birdsong {

template<typename T, unsigned Cap>
  requires(Cap >= 1)
class Rotbuf
{
public:
  struct iterator
  {
    T* at;
    T* end;

    T& operator*() { return *at; }
    iterator& operator++()
    {
      at++;
      if (at >= end)
        at = end - sizeof(T) * Cap;
    }

    bool operator==(iterator const& rhs) const { return at == rhs.at; }
  };

  Rotbuf(iterator begin, iterator end) { m_size = end - begin; }

  ~Rotbuf()
  {
    for (auto& val : *this)
      val.~T();
  }

  iterator begin() { return iterator{ m_arr.begin() + m_idx, m_arr.end() }; }
  iterator end()
  {
    return iterator{ m_arr.begin() + ((m_idx + m_size) % Cap), m_arr.end() };
  }

  T& push() {}

private:
  T& at(unsigned i) { return m_arr[i % Cap]; }

  unsigned m_idx{ 0 };
  unsigned m_size{ 0 };
  alignas(alignof(T)) std::array<std::byte[sizeof(T)], Cap> m_arr;
};

};
