#pragma once

#include <compare>
namespace birdsong {

struct IPAddr
{
public:
  explicit IPAddr(unsigned, unsigned short port);
  std::strong_ordering operator<=>(IPAddr const& rhs) const;
  unsigned char operator[](int i) const;

private:
  unsigned m_val;
  unsigned short m_port;
};

};
