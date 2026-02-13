
#include <gtest/gtest.h>
#include <fstream>

#include "database_fixture.hpp"
#include "database/database.hpp"
#include "database/pages/btree.hpp"

TEST_F(TempFileFixture, CreateDatabase)
{
  std::fstream f;
  f.open(path, std::ios::in | std::ios::out | std::ios::binary | std::ios::app);
  Database db = Database(f);
  Page<> &p = db.pager.getPage(0);
  EXPECT_EQ(PageType::First, p.header()->type);
}

TEST(Pager, GetPageInPlace)
{
  std::stringstream ss;
  Database db = Database(ss);
  Page<> &p1 = db.pager.getPage(0);
  Page<> &p2 = db.pager.getPage(0);
  p1.buf[0] = static_cast<std::byte>(123);
  EXPECT_EQ(&p1, &p2);
  EXPECT_EQ(p1.buf[0], p2.buf[0]);
  EXPECT_EQ(static_cast<std::byte>(123), p2.buf[0]);
}

TEST(Pager, AsRef)
{
  std::stringstream mockStream;
  Pager pager = Pager(mockStream);
  InteriorNode i{pager.fromNextFree<BTreeHeader>(Interior)};
  Page<> &p = i.page.as_ref<CommonHeader>();
  EXPECT_EQ(PageType::Interior, i.page.header()->common.type);
  EXPECT_EQ(PageType::Interior, p.header()->type);
}

/* nodes should retain their data when being inserted/retrieved from the pager */
TEST(Pager, NodeTypes)
{
  std::stringstream mockStream;
  Pager pager = Pager(mockStream);

  PageId leafId{};
  Page<BTreeHeader> &leafPage = pager.fromNextFree<BTreeHeader>(PageType::Leaf, &leafId);
  LeafNode leaf{leafPage};

  PageId interiorId{};
  auto &interiorPage = pager.fromNextFree<BTreeHeader>(PageType::Interior, &interiorId);
  InteriorNode interior{interiorPage};

  leaf.page.header()->slots.insertCell(LeafCell(static_cast<u32>(2)));

  Page<BTreeHeader> &l2 = pager.getPage<BTreeHeader>(leafId);
  Page<BTreeHeader> &i2 = pager.getPage<BTreeHeader>(interiorId);
  EXPECT_EQ(PageType::Leaf, l2.header()->common.type);
  EXPECT_EQ(PageType::Interior, i2.header()->common.type);
  EXPECT_EQ(leaf.page.buf, l2.buf);
  EXPECT_EQ(interior.page.buf, i2.buf);
}

TEST(Database, InMemory)
{
  std::stringstream ss;
  Database db = Database(ss);
  Page<> &p = db.pager.getPage(0);
  EXPECT_EQ(PageType::First, p.header()->type);
}

/* the database free list is a linked list of the entirely free pages on disk.
 * marking a page as free sets it as the head of the linked list and sets its pointer to the
 * previous head */
TEST_F(TempFileFixture, Freelist)
{
  std::fstream f;
  f.open(path, std::ios::in | std::ios::out | std::ios::binary | std::ios::app);
  Database db = Database(f);
  Page<FirstPage::Header> &firstPage = db.pager.getPage<FirstPage::Header>(static_cast<PageId>(0));

  EXPECT_EQ(0, firstPage.header()->db.freelist);
  PageId a = db.pager.nextFree();
  EXPECT_EQ(1, a);
  EXPECT_EQ(PAGE_SIZE * 2, db.pager.fsize());

  PageId b = db.pager.nextFree();
  EXPECT_EQ(2, b);
  EXPECT_EQ(PAGE_SIZE * 3, db.pager.fsize());

  // freeing a page marks it as free and sets the linked list
  Page<> &pageA = db.pager.getPage(a);
  pageA.header()->type = PageType::Interior;
  db.pager.freePage(a);
  EXPECT_EQ(PageType::Freelist, pageA.header()->type);
  EXPECT_EQ(a, firstPage.header()->db.freelist);

  // reuse previously freed page
  a = db.pager.nextFree();
  EXPECT_EQ(1, a);
  EXPECT_EQ(0, firstPage.header()->db.freelist);
  EXPECT_EQ(PAGE_SIZE * 3, db.pager.fsize());
  const Page<> &freeA = db.pager.getPage(a);
  EXPECT_EQ(PageType::Freelist, freeA.header()->type);


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
