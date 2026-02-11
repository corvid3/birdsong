#pragma once

#include <compare>
namespace birdsong {

struct IPAddr
{
public:
  explicit IPAddr(unsigned, unsigned short port);
  std::strong_ordering operator<=>(IPAddr const& rhs) const;
  unsigned char operator[](int i) const;

  auto addr() const { return m_val; }
  auto port() const { return m_port; }

private:
  unsigned m_val;
  unsigned short m_port;
};

};
