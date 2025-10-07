
#include <gtest/gtest.h>
#include <filesystem>

#include "database/database.hpp"

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

TEST(Slots, AddCells)
{
  // test cells in the slots
  std::array<char, 128> buf;
  SlotHeader *sh = reinterpret_cast<SlotHeader*>(buf.data());
  sh->freeLength = buf.size() - sizeof(SlotHeader);
  sh->freeStart = 0;

  LeafCell cell;
  cell.cell.key = 123;
  cell.cell.payloadSize = 20;
  cell.data = std::make_unique<char[]>(cell.cell.payloadSize);
  const int totalCellSize = sizeof(cell.cell) + cell.cell.payloadSize;
  u16 slotNumber;
  sh->newCell(totalCellSize, &slotNumber); 

  Slot slot = *sh->getSlot(slotNumber);

  EXPECT_EQ(totalCellSize, slot.cellSize);
  EXPECT_EQ(0, slotNumber);
  EXPECT_EQ(buf.size() - totalCellSize, slot.cellOffset);
  EXPECT_EQ(buf.size() - sizeof(SlotHeader) - sizeof(Slot) - totalCellSize, sh->freeLength);
  EXPECT_EQ(sizeof(Slot), sh->freeStart);

  sh->newCell(totalCellSize, &slotNumber);
  slot = *sh->getSlot(slotNumber);
  EXPECT_EQ(1, slotNumber);
  EXPECT_EQ(buf.size() - 2*totalCellSize, slot.cellOffset);
  EXPECT_EQ(buf.size() - sizeof(SlotHeader) - 2*sizeof(Slot) - 2*totalCellSize, sh->freeLength);
  EXPECT_EQ(2*sizeof(Slot), sh->freeStart);
}

TEST(Slots, AddThenRead)
{
  // test cells in the slots
  std::array<char, 128> buf;
  SlotHeader *sh = reinterpret_cast<SlotHeader*>(buf.data());
  sh->freeLength = buf.size() - sizeof(SlotHeader);
  sh->freeStart = 0;

  LeafCell cell;
  cell.cell.key = 123;
  cell.cell.payloadSize = 20;
  cell.data = std::make_unique<char[]>(cell.cell.payloadSize);
  const int totalCellSize = sizeof(cell.cell) + cell.cell.payloadSize;

  Slot slot;
  u16 slotNumber;
  LeafCell::Cell *pCell = reinterpret_cast<LeafCell::Cell*>(sh->newCell(totalCellSize, &slotNumber)); 
  *pCell = cell.cell;

  LeafCell::Cell readCell = *reinterpret_cast<LeafCell::Cell*>(sh->getSlotCell(slotNumber, &slot));

  EXPECT_EQ(reinterpret_cast<intptr_t>(pCell), reinterpret_cast<intptr_t>(sh + 1) + slot.cellOffset);
  EXPECT_EQ(cell.cell.key, readCell.key);
  EXPECT_EQ(cell.cell.payloadSize, readCell.payloadSize);
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
