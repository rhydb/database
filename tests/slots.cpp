#include <gtest/gtest.h>
#include <span>
#include "database/pages/btree.hpp"

/* values are inserted and retrieved in sorted order */
TEST(SlotsIterator, ValuesSorted)
{
  std::array<std::byte, 512> buf = {static_cast<std::byte>(0)};
  constexpr std::size_t bufsize = buf.size() - sizeof(SlotHeader<0>);
  SlotHeader<bufsize> &sh = *reinterpret_cast<SlotHeader<bufsize> *>(buf.data());
  sh = SlotHeader<bufsize>();
  sh.insertCell(LeafCell(static_cast<u32>(3)));
  sh.insertCell(LeafCell(static_cast<u32>(2)));
  sh.insertCell(LeafCell(static_cast<u32>(1)));

  int expected = 1;
  for (const Slot &s : sh)
  {
    LeafCell<u32> c = *reinterpret_cast<const LeafCell<u32> *>(sh.readCell(s.cellOffset));
    u32 v = c.getPayload();
    EXPECT_EQ(expected, v);
    ++expected;
  }
  EXPECT_EQ(4, expected);
}

/* End slots are iterated over last */
TEST(SlotsIterator, IterateEndSlot)
{
  std::array<std::byte, 512> buf = {static_cast<std::byte>(0)};
  constexpr std::size_t bufsize = buf.size() - sizeof(SlotHeader<0>);
  SlotHeader<bufsize> &sh = *reinterpret_cast<SlotHeader<bufsize> *>(buf.data());
  sh = SlotHeader<bufsize>();
  sh.insertCell(InteriorCell<u32>::End());
  sh.insertCell(InteriorCell(static_cast<u32>(1)));
  sh.insertCell(InteriorCell(static_cast<u32>(2)));
  sh.insertCell(InteriorCell(static_cast<u32>(3)));

  int expected = 1;
  for (const Slot &s : sh)
  {
    InteriorCell c = *reinterpret_cast<const InteriorCell<u32> *>(sh.readCell(s.cellOffset));
    u32 v = c.cell.getPayload();

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
  constexpr std::size_t bufsize = buf.size() - sizeof(SlotHeader<0>);
  SlotHeader<bufsize> &sh = *reinterpret_cast<SlotHeader<bufsize> *>(buf.data());
  sh = SlotHeader<bufsize>();

  const Slot *slot = sh.insertCell(LeafCell(static_cast<u32>(3)));

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
