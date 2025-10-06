#pragma once

#include <iostream>
#include <cstdint>
#include <array>

#define sizeof_attr(Struct, Attribute) sizeof(((Struct*)0)->Attribute)

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;

inline void writeNetworku8(std::ostream &out, u8 v)
{
  char c = static_cast<char>(v);
  out.write(&c, 1);
}

inline bool readNetworku8(std::istream &in, u8 &v)
{
  char c;
  in.read(&c, 1);
  if (!in)
    return false;

  v = static_cast<u8>(static_cast<unsigned char>(c));
  return true;
}

inline void writeNetworku16(std::ostream &out, u16 v)
{
  unsigned char buf[] = {
      static_cast<unsigned char>((v >> 8) & 0xFF),
      static_cast<unsigned char>((v >> 0) & 0xFF),
  };

  out.write(reinterpret_cast<const char *>(buf), 2);
}

inline bool readNetworku16(std::istream &in, u16 &v)
{
  std::array<unsigned char, 2> buf;
  in.read(reinterpret_cast<char *>(buf.data()), static_cast<std::streamsize>(buf.size()));
  if (!in)
    return false;

  v = (static_cast<u32>(buf[0]) << 8) |
      (static_cast<u32>(buf[1]));
  return true;
}

inline bool writeNetworku32(std::ostream &out, u32 v)
{
  unsigned char buf[] = {
      static_cast<unsigned char>((v >> 24) & 0xFF),
      static_cast<unsigned char>((v >> 16) & 0xFF),
      static_cast<unsigned char>((v >> 8) & 0xFF),
      static_cast<unsigned char>((v >> 0) & 0xFF),
  };

  if (!out.write(reinterpret_cast<const char *>(buf), 4))
    return false;
  return true;
}

inline bool readNetworku32(std::istream &in, u32 &v)
{
  std::array<unsigned char, 4> buf;
  in.read(reinterpret_cast<char *>(buf.data()), static_cast<std::streamsize>(buf.size()));
  if (!in)
    return false;

  v = (static_cast<u32>(buf[0]) << 24) |
      (static_cast<u32>(buf[1]) << 16) |
      (static_cast<u32>(buf[2]) << 8) |
      (static_cast<u32>(buf[3]));
  return true;
}

template<std::size_t N>
inline void writeNetworku32(std::ostream &out, const std::array<u32, N> &in_array)
{
  for (std::size_t i = 0; i < N; i++)
  {
    writeNetworku32(out, in_array[i]);
  }
}


template<std::size_t N>
inline bool readNetworku32(std::istream &in, std::array<u32, N> &out_array)
{ 
  for (std::size_t i = 0; i < N; i++)
  {
    if (!readNetworku32(in, out_array[i]))
      return false;
  }

  return true;
}

class membuf : public std::streambuf
{
public:
  membuf(char *data, std::size_t N)
  {
    setg(data, data, data + N);
    setp(data, data + N);
  }
};
