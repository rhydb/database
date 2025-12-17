#include <gtest/gtest.h>
#include <span>
#include "database/pages/btree.hpp"

/* values are inserted and retrieved in sorted order */
TEST(SlotsIterator, ValuesSorted)
{
  std::array<std::byte, 512> buf = {static_cast<std::byte>(0)};
  SlotHeader sh = SlotHeader(std::span(buf.data(), buf.size()));
  sh.insertCell<u32>(LeafCell(static_cast<u32>(3)));
  sh.insertCell<u32>(LeafCell(static_cast<u32>(2)));
  sh.insertCell<u32>(LeafCell(static_cast<u32>(1)));

  int expected = 1;
  for (const Slot &s : sh)
  {
    LeafCell c = *reinterpret_cast<const LeafCell *>(sh.readCell(s.cellOffset));
    u32 v = *reinterpret_cast<u32 *>(c.payload.small.data());
    EXPECT_EQ(expected, v);
    ++expected;
  }
  EXPECT_EQ(4, expected);
}

/* End slots are iterated over last */
TEST(SlotsIterator, IterateEndSlot)
{
  std::array<std::byte, 512> buf = {static_cast<std::byte>(0)};
  SlotHeader sh = SlotHeader(std::span(buf.data(), buf.size()));
  sh.insertCell<u32>(InteriorCell::End());
  sh.insertCell<u32>(InteriorCell(static_cast<u32>(1)));
  sh.insertCell<u32>(InteriorCell(static_cast<u32>(2)));
  sh.insertCell<u32>(InteriorCell(static_cast<u32>(3)));

  int expected = 1;
  for (const Slot &s : sh)
  {
    InteriorCell c = *reinterpret_cast<const InteriorCell *>(sh.readCell(s.cellOffset));
    u32 v = *reinterpret_cast<u32 *>(c.cell.payload.small.data());

    if (c.isEnd())
    {
      EXPECT_EQ(0, v);
      EXPECT_EQ(4, expected);
    }
    else
    {
      EXPECT_EQ(expected, v);
    }

    ++expected;
  }
  EXPECT_EQ(5, expected);
}

/* The slot references the actual slot in the page */
TEST(SlotsIterator, ReferenceAddress)
{
  std::array<std::byte, 512> buf = {static_cast<std::byte>(0)};
  SlotHeader sh = SlotHeader(std::span(buf.data(), buf.size()));

  const Slot *slot = sh.insertCell<u32>(LeafCell(static_cast<u32>(3)));

  int i = 0;
  for (const Slot &s : sh)
  {
    EXPECT_EQ(*slot, s);
    EXPECT_EQ(slot, &s);
    ++i;
  }
  EXPECT_EQ(1, i);

  i = 0;
  for (const Slot s : sh)
  {
    EXPECT_EQ(*slot, s);
    EXPECT_TRUE(slot != &s);
    ++i;
  }
  EXPECT_EQ(1, i);
}
