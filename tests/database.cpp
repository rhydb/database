
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>

#include "database/database.hpp"
#include "database/pages/btree.hpp"

class TempFileFixture : public ::testing::Test
{
protected:
  void SetUp() override
  {
    m_name = makeUniqueName();
    path = std::filesystem::temp_directory_path() / m_name;
  }

  void TearDown() override {
    if (HasFailure())
    {
      std::cerr << "Failed database file at: " << path << std::endl;
    }
    else
    {
      if (std::filesystem::exists(path))
      {
        std::filesystem::remove(path);
      }
    }
  }

  std::filesystem::path path;
    
private:
    std::string makeUniqueName()
    {
      auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
#if defined(_WIN32)
      unsigned long pid = GetCurrentProcessId();
#else
      unsigned long pid = static_cast<unsigned long>(::getpid());
#endif
      char buf[128];
      std::snprintf(buf, sizeof(buf), "test_tmp_%lx_%lx", now, pid);
      return std::string(buf);
    }

    std::string m_name;
};

TEST_F(TempFileFixture, CreateDatabase) {
  std::fstream f;
  f.open(path, std::ios::in | std::ios::out | std::ios::binary | std::ios::app);
  Database db = Database(f);
  Page<> &p = db.pager.getPage(0);
  EXPECT_EQ(PageType::First, p.header()->type);
}

TEST(Database, InMemory) {
  std::stringstream ss;
  Database db = Database(ss);
  Page<> &p = db.pager.getPage(0);
  EXPECT_EQ(PageType::First, p.header()->type);
}

/* the database free list is a linked list of the entirely free pages on disk.
 * marking a page as free sets it as the head of the linked list and sets its pointer to the previous head */
TEST_F(TempFileFixture, Freelist) {
  std::fstream f;
  f.open(path, std::ios::in | std::ios::out | std::ios::binary | std::ios::app);
  Database db = Database(f);
  Page<FirstPage::Header> &firstPage = db.pager.getPage(0).as<FirstPage::Header>();

  EXPECT_EQ(0, firstPage.header()->db.freelist);
  PageId a = db.pager.nextFree();
  EXPECT_EQ(1, a);
  EXPECT_EQ(PAGE_SIZE*2, db.pager.fsize());

  PageId b = db.pager.nextFree();
  EXPECT_EQ(2, b);
  EXPECT_EQ(PAGE_SIZE*3, db.pager.fsize());

  // freeing a page marks it as free and sets the linked list
  Page<> &pageA = db.pager.getPage(a);
  pageA.header()->type = PageType::Root;
  db.pager.freePage(a);
  EXPECT_EQ(PageType::Freelist, pageA.header()->type);
  EXPECT_EQ(a, firstPage.header()->db.freelist);

  // reuse previously freed page
  a = db.pager.nextFree();
  EXPECT_EQ(1, a);
  EXPECT_EQ(0, firstPage.header()->db.freelist);
  EXPECT_EQ(PAGE_SIZE*3, db.pager.fsize());

  db.pager.freePage(a);
  db.pager.freePage(b);

  // the list should be in reverse order
  EXPECT_EQ(b, firstPage.header()->db.freelist);
  b = db.pager.nextFree();
  EXPECT_EQ(2, b);
  EXPECT_EQ(a, firstPage.header()->db.freelist);
  a = db.pager.nextFree();
  EXPECT_EQ(1, a);
  EXPECT_EQ(0, firstPage.header()->db.freelist);
}

/* add slots with cell data and make sure the free start and length pointers are updated */
TEST(Slots, AddSlotsAndCellsUpdatesFreePointers)
{
  // test cells in the slots
  std::array<std::byte, 128> buf;
  SlotHeader sh = SlotHeader(std::span(buf.data(), buf.size()));

  LeafCell cell;
  cell.payloadSize = 1;
  cell.data.smallPayload[0] = static_cast<std::byte>(123);
  
  {
    u16 slotNumber;
    Slot *slot = sh.createNextSlot(cell.cellSize(), &slotNumber);
    slot->cellOffset = sh.createNextCell(cell.cellSize());

    EXPECT_EQ(cell.cellSize(), slot->cellSize);
    EXPECT_EQ(0, slotNumber);
    EXPECT_EQ(buf.size() - cell.cellSize(), slot->cellOffset);
    EXPECT_EQ(buf.size() - sizeof(Slot) - cell.cellSize(), sh.freeLength);
    EXPECT_EQ(sizeof(Slot), sh.freeStart);
  }

  {
    u16 slotNumber2;
    Slot *slot2 = sh.createNextSlot(cell.cellSize(), &slotNumber2);
    slot2->cellOffset = sh.createNextCell(cell.cellSize());
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
  std::array<std::byte, 128> buf;
  SlotHeader sh = SlotHeader(std::span(buf.data(), buf.size()));

  LeafCell cell;
  cell.data.smallPayload[0] = static_cast<std::byte>(123);
  cell.payloadSize = 1;
  ASSERT_EQ(sizeof(u32) + 1, cell.cellSize());

  // create the slot and its cell
  u16 slotNumber;
  Slot *slot = sh.createNextSlot(cell.cellSize(), &slotNumber);
  slot->cellOffset = sh.createNextCell(cell.cellSize());
  // set the contents of the cell
  LeafCell *pCell = reinterpret_cast<LeafCell*>(sh.getCell(slot->cellOffset)); 
  std::memcpy(pCell, &cell, cell.cellSize());

  // read the slot and cell back using the slot number
  Slot readSlot;
  LeafCell readCell = *reinterpret_cast<LeafCell*>(sh.getSlotAndCell(slotNumber, &readSlot));

  EXPECT_EQ(reinterpret_cast<intptr_t>(pCell), reinterpret_cast<intptr_t>(buf.data() + slot->cellOffset));
  EXPECT_EQ(cell.payloadSize, readCell.payloadSize);
  EXPECT_EQ(cell.data.smallPayload[0], readCell.data.smallPayload[0]);
}

TEST(Slots, OutOfBounds)
{
  std::array<std::byte, 128> buf;
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
  std::array<std::byte, 128> buf;
  SlotHeader sh = SlotHeader(std::span(buf.data(), buf.size()));

  struct Cell {
    u16 totalSize;
    std::array<char, 10> data;
  };
  // sort the slots keys lexicographically
  auto comparitor = std::function([](const Cell &a, const Cell &b){
        return std::strncmp(a.data.data(), b.data.data(), a.data.size()) < 0;
  });
  Cell c;
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
  std::array<std::byte, 128> buf;
  SlotHeader sh = SlotHeader(std::span(buf.data(), buf.size()));
  
  struct Cell {
    u16 totalSize;
    std::array<char, 10> data;
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

// TEST(Database, InsertSelect) {
//   Table t = Table("test.db");
//   Row r = {1, {"username"}, {"email"}};
//   t.insert(r);
//
//   ASSERT_EQ(t.nRows(), 1);
//
//   Row r2;
//
//   std::stringbuf buf(std::string(t.rowSlot(0), ROW_SIZE), std::ios::in | std::ios::out | std::ios_base::binary);
//   std::iostream ss(&buf);
//
//   r2.deserialise(ss);
//   std::cout << r2.id << "," << r2.username.data() << "," << r2.email.data() << std::endl;
//   EXPECT_EQ(r.username, r2.username);
//   EXPECT_EQ(r.email, r2.email);
// }
