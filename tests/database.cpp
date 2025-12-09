
#include <gtest/gtest.h>
#include <fstream>

#include "database_fixture.hpp"
#include "database/database.hpp"
#include "database/pages/btree.hpp"

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
