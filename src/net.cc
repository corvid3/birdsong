
#include "net.hh"

birdsong::IPAddr::IPAddr(unsigned v, unsigned short port)
  : m_val(v)
  , m_port(port) {};

std::strong_ordering
birdsong::IPAddr::operator<=>(IPAddr const& rhs) const
{
  auto const addr = m_val <=> rhs.m_val;
  if (addr != std::strong_ordering::equal)
    return addr;
  return m_port <=> rhs.m_port;
}

unsigned char
birdsong::IPAddr::operator[](int i) const
{
  return (m_val >> (i * 8)) & 0xFF;
}
