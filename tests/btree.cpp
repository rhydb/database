#include <gtest/gtest.h>

#include "database/pages/btree.hpp"

/* add slots with cell data and make sure the free start and length pointers are updated */
TEST(Slots, AddSlotsAndCellsUpdatesFreePointers)
{
  // test cells in the slots
  std::array<std::byte, 128> buf = {static_cast<std::byte>(0)};
  SlotHeader sh = SlotHeader(std::span(buf.data(), buf.size()));

  NodeCell cell(static_cast<u32>(123));
  
  {
    u16 slotNumber;
    Slot *slot = sh.createNextSlot(cell.cellSize(), &slotNumber);
    slot->cellOffset = sh.allocNextCell(cell.cellSize());

    EXPECT_EQ(cell.cellSize(), slot->cellSize);
    EXPECT_EQ(0, slotNumber);
    EXPECT_EQ(buf.size() - cell.cellSize(), slot->cellOffset);
    EXPECT_EQ(buf.size() - sizeof(Slot) - cell.cellSize(), sh.freeLength);
    EXPECT_EQ(sizeof(Slot), sh.freeStart);
  }

  {
    u16 slotNumber2;
    Slot *slot2 = sh.createNextSlot(cell.cellSize(), &slotNumber2);
    slot2->cellOffset = sh.allocNextCell(cell.cellSize());
    EXPECT_EQ(1, slotNumber2);
    EXPECT_EQ(buf.size() - 2*cell.cellSize(), slot2->cellOffset);
    EXPECT_EQ(buf.size() - 2*sizeof(Slot) - 2*cell.cellSize(), sh.freeLength);
    EXPECT_EQ(2*sizeof(Slot), sh.freeStart);
  }
}

/* add a slot and cell with some data, then read the cell data back using just the slot */
TEST(Slots, AddThenRead)
{
  // test cells in the slots
  std::array<std::byte, 128> buf = {static_cast<std::byte>(0)};
  SlotHeader sh = SlotHeader(std::span(buf.data(), buf.size()));

  NodeCell cell(static_cast<u32>(123));
  ASSERT_EQ(sizeof(u32) + sizeof(u32), cell.cellSize());

  // create the slot and its cell
  u16 slotNumber;
  Slot *slot = sh.createNextSlot(cell.cellSize(), &slotNumber);
  slot->cellOffset = sh.allocNextCell(cell.cellSize());
  // set the contents of the cell
  NodeCell *pCell = reinterpret_cast<NodeCell*>(sh.getCell(slot->cellOffset)); 
  std::memcpy(pCell, &cell, cell.cellSize());

  // read the slot and cell back using the slot number
  Slot readSlot;
  NodeCell readCell = *reinterpret_cast<NodeCell*>(sh.getSlotAndCell(slotNumber, &readSlot));

  EXPECT_EQ(reinterpret_cast<intptr_t>(pCell), reinterpret_cast<intptr_t>(buf.data() + slot->cellOffset));
  EXPECT_EQ(cell.payloadSize, readCell.payloadSize);
  EXPECT_EQ(cell.payload.small[0], readCell.payload.small[0]);
}

TEST(Slots, OutOfBounds)
{
  std::array<std::byte, 128> buf = {static_cast<std::byte>(0)};
  SlotHeader sh = SlotHeader(std::span(buf.data(), buf.size()));

  // allow getting the slot on the boundary of freeStart as that's how we get new cells
  ASSERT_NO_THROW({
      sh.getSlot(0);
  });

  ASSERT_THROW({
      sh.getSlot(1);
  }, std::out_of_range);
}

/* test that cells and slots are inserted correctly into the slotted page and that the data is read back correctly.
 * we also test that the sorting places the slots into the correct place */
TEST(Slots, Insertion)
{
  std::array<std::byte, 128> buf = {static_cast<std::byte>(0)};
  SlotHeader sh = SlotHeader(std::span(buf.data(), buf.size()));

  struct Cell {
    u16 totalSize;
    std::array<char, 10> data = {0};
  };
  // sort the slots keys lexicographically
  auto comparitor = std::function([](const Cell &a, const Cell &b){
        return std::strncmp(a.data.data(), b.data.data(), a.data.size()) < 0;
  });
  Cell c = Cell();
  std::strcpy(c.data.data(), "key1");
  c.totalSize = c.data.size();
  u16 slotNumber;
  Slot *s = sh.insertCell(c, comparitor, &slotNumber);
  ASSERT_TRUE(s != nullptr);

  EXPECT_EQ(0, slotNumber);
  EXPECT_EQ(sh.freeStart + sh.freeLength, s->cellOffset);
  EXPECT_EQ(sizeof(c), s->cellSize);
  Cell *pc = reinterpret_cast<Cell *>(sh.getCell(s->cellOffset));

  EXPECT_EQ(c.totalSize, pc->totalSize);
  EXPECT_EQ(c.data, pc->data);

  std::strcpy(c.data.data(), "key2");
  Slot *s2 = sh.insertCell(c, comparitor, &slotNumber);
  Cell *pc2 = reinterpret_cast<Cell *>(sh.getCell(s2->cellOffset));
  EXPECT_EQ(c.data, pc2->data);
  EXPECT_EQ(1, slotNumber);

  std::strcpy(c.data.data(), "key0");
  Slot *s3 = sh.insertCell(c, comparitor, &slotNumber);
  Cell *pc3 = reinterpret_cast<Cell *>(sh.getCell(s3->cellOffset));
  EXPECT_EQ(c.data, pc3->data);
  EXPECT_EQ(0, slotNumber);
}

/* test that data can be inserted, deleted, and reinserted into the same slot
 * this checks that the sorted order is maintained when reinserting into a previous position */
TEST(Slots, InsertAfterDelete)
{
  std::array<std::byte, 128> buf = {static_cast<std::byte>(0)};
  SlotHeader sh = SlotHeader(std::span(buf.data(), buf.size()));
  
  struct Cell {
    u16 totalSize;
    std::array<char, 10> data = {0};
  };
  // sort the keys lexicographically
  auto comparitor = std::function([](const Cell &a, const Cell &b){
        return std::strncmp(a.data.data(), b.data.data(), a.data.size()) < 0;
  });


  Cell c;
  std::strcpy(c.data.data(), "key1");
  c.totalSize = c.data.size();
  sh.insertCell(c, comparitor);
  // skip key2 so that we can insert it later
  // we need a different value that is also larger than key1
  SlotNum slotNumToDelete;
  std::strcpy(c.data.data(), "key3");
  sh.insertCell(c, comparitor, &slotNumToDelete);
  std::strcpy(c.data.data(), "key4");
  Slot key4 = *sh.insertCell(c, comparitor);

  const u16 freeStart = sh.freeStart;
  const u16 freeLength = sh.freeLength;
  sh.deleteSlot(slotNumToDelete);
  // the old cell data is still there
  EXPECT_EQ(freeStart - sizeof(Slot), sh.freeStart);
  EXPECT_EQ(freeLength + sizeof(Slot), sh.freeLength);

  // the slot should have moved down
  Slot *s = sh.getSlot(slotNumToDelete);
  EXPECT_EQ(key4.cellOffset, s->cellOffset);
  EXPECT_EQ(key4.cellSize, s->cellSize);

  SlotNum slotNumUsed;
  std::strcpy(c.data.data(), "key2");
  sh.insertCell(c, comparitor, &slotNumUsed);
  EXPECT_EQ(slotNumToDelete, slotNumUsed);
  EXPECT_EQ(freeStart, sh.freeStart);
  // the free length has not increased again because the old cell data is still there
  EXPECT_EQ(freeLength - sizeof(Cell), sh.freeLength);
}
