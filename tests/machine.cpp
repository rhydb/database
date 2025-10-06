#include <gtest/gtest.h>

#include "machine.hpp"

TEST(NetworkU32, WriteThenRead) {
    std::ostringstream out;
    std::uint32_t original = 0xDEADBEEF;
    writeNetworku32(out, original);

    std::string bytes = out.str();
    ASSERT_EQ(bytes.size(), 4u);

    // Expect specific big-endian byte sequence
    EXPECT_EQ(static_cast<std::uint8_t>(bytes[0]), 0xDE);
    EXPECT_EQ(static_cast<std::uint8_t>(bytes[1]), 0xAD);
    EXPECT_EQ(static_cast<std::uint8_t>(bytes[2]), 0xBE);
    EXPECT_EQ(static_cast<std::uint8_t>(bytes[3]), 0xEF);

    std::istringstream in(bytes, std::ios_base::binary);
    uint32_t reconstructed = 0;
    ASSERT_TRUE(readNetworku32(in, reconstructed));
    EXPECT_EQ(reconstructed, original);
}

TEST(NetworkU32, ReadFailsOnShortStream) {
  // Only 3 bytes available -> read should fail and leave `out` unchanged
  std::string short_bytes = std::string("\x01\x02\x03", 3);
  std::istringstream in(short_bytes, std::ios_base::binary);
  std::uint32_t out_val = 0xFFFFFFFF;
  EXPECT_FALSE(readNetworku32(in, out_val));
  // out_val must remain unchanged
  EXPECT_EQ(out_val, 0xFFFFFFFFu);
}

TEST(NetworkU32, MultipleReads) {
  std::ostringstream out;
  writeNetworku32(out, 0x00000001);
  writeNetworku32(out, 0x7F800001);
  writeNetworku32(out, 0xFFFFFFFF);

  std::istringstream in(out.str(), std::ios_base::binary);
  std::uint32_t v1, v2, v3;
  ASSERT_TRUE(readNetworku32(in, v1));
  ASSERT_TRUE(readNetworku32(in, v2));
  ASSERT_TRUE(readNetworku32(in, v3));
  EXPECT_EQ(v1, 0x00000001u);
  EXPECT_EQ(v2, 0x7F800001u);
  EXPECT_EQ(v3, 0xFFFFFFFFu);

  // EOF now: additional read should fail
  std::uint32_t v4 = 0;
  EXPECT_FALSE(readNetworku32(in, v4));
}

TEST(NetworkU32, WriteReadArray)
{
  const std::array<std::uint32_t, 3> src = {0u, 0xDEADBEEFu, 0xFFFFFFFFu};
  std::ostringstream out;
  writeNetworku32(out, src);

  std::string bytes = out.str();
  ASSERT_EQ(bytes.size(), src.size() * sizeof(std::uint32_t));

  std::istringstream in(bytes, std::ios_base::binary);
  std::array<std::uint32_t, 3> dst{};
  ASSERT_TRUE(readNetworku32(in, dst));
  EXPECT_EQ(dst, src);
}
