#include <gtest/gtest.h>

#include "database/pages/btree.hpp"

/* add slots with cell data and make sure the free start and length pointers are updated */
TEST(Slots, AddSlotsAndCellsUpdatesFreePointers)
{
  // test cells in the slots
  std::array<std::byte, 128> page = {static_cast<std::byte>(0)};
  constexpr std::size_t bufsize = page.size() - sizeof(SlotHeader<0>);
  SlotHeader<bufsize> &sh = *reinterpret_cast<SlotHeader<bufsize> *>(page.data());
  sh = SlotHeader<bufsize>();

  // the slot header lives inside the page, so create a span to exclude the header
  std::span<std::byte> buf =
      std::span<std::byte>(page.data() + sizeof(sh), page.size() - sizeof(sh));

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
    EXPECT_EQ(buf.size() - 2 * cell.cellSize(), slot2->cellOffset);
    EXPECT_EQ(buf.size() - 2 * sizeof(Slot) - 2 * cell.cellSize(), sh.freeLength);
    EXPECT_EQ(2 * sizeof(Slot), sh.freeStart);
  }
}

/* add a slot and cell with some data, then read the cell data back using just the slot */
TEST(Slots, AddThenRead)
{
  // test cells in the slots
  std::array<std::byte, 128> page = {static_cast<std::byte>(0)};
  constexpr std::size_t bufsize = page.size() - sizeof(SlotHeader<0>);
  SlotHeader<bufsize> &sh = *reinterpret_cast<SlotHeader<bufsize> *>(page.data());
  sh = SlotHeader<bufsize>();

  // the slot header lives inside the page, so create a span to exclude the header
  std::span<std::byte> buf =
      std::span<std::byte>(page.data() + sizeof(sh), page.size() - sizeof(sh));

  NodeCell cell(static_cast<u32>(123));
  ASSERT_EQ(sizeof(u32) + sizeof(u32), cell.cellSize());

  // create the slot and its cell
  u16 slotNumber;
  Slot *slot = sh.createNextSlot(cell.cellSize(), &slotNumber);
  slot->cellOffset = sh.allocNextCell(cell.cellSize());
  // set the contents of the cell
  NodeCell<u32> *pCell = reinterpret_cast<NodeCell<u32> *>(sh.getCell(slot->cellOffset));
  std::memcpy(pCell, &cell, cell.cellSize());

  // read the slot and cell back using the slot number
  Slot readSlot;
  NodeCell<u32> readCell =
      *reinterpret_cast<NodeCell<u32> *>(sh.getSlotAndCell(slotNumber, &readSlot));

  EXPECT_EQ(reinterpret_cast<intptr_t>(pCell),
            reinterpret_cast<intptr_t>(buf.data() + slot->cellOffset));
  EXPECT_EQ(cell.payloadSize, readCell.payloadSize);
  EXPECT_EQ(cell.getPayload(), readCell.getPayload());
}

TEST(Slots, OutOfBounds)
{
  std::array<std::byte, 128> buf = {static_cast<std::byte>(0)};
  constexpr std::size_t bufsize = buf.size() - sizeof(SlotHeader<0>);
  SlotHeader<bufsize> &sh = *reinterpret_cast<SlotHeader<bufsize> *>(buf.data());
  sh = SlotHeader<bufsize>();

  // allow getting the slot on the boundary of freeStart as that's how we get new cells
  ASSERT_NO_THROW({ sh.getSlot(0); });

  ASSERT_THROW({ sh.getSlot(1); }, std::out_of_range);
}

/* test that cells and slots are inserted correctly into the slotted page and that the data is read
 * back correctly. we also test that the sorting places the slots into the correct place */
TEST(Slots, Insertion)
{
  std::array<std::byte, 128> buf = {static_cast<std::byte>(0)};
  constexpr std::size_t bufsize = buf.size() - sizeof(SlotHeader<0>);
  SlotHeader<bufsize> &sh = *reinterpret_cast<SlotHeader<bufsize> *>(buf.data());
  sh = SlotHeader<bufsize>();

  struct Cell
  {
    u16 totalSize;
    std::array<char, 10> data = {0};
  };
  // sort the slots keys lexicographically
  auto comparitor =
      std::function([](const Cell &a, const Cell &b)
                    { return std::strncmp(a.data.data(), b.data.data(), a.data.size()) < 0; });
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
  constexpr std::size_t bufsize = buf.size() - sizeof(SlotHeader<0>);
  SlotHeader<bufsize> &sh = *reinterpret_cast<SlotHeader<bufsize> *>(buf.data());
  sh = SlotHeader<bufsize>();

  struct Cell
  {
    u16 totalSize;
    std::array<char, 10> data = {0};
  };
  // sort the keys lexicographically
  auto comparitor =
      std::function([](const Cell &a, const Cell &b)
                    { return std::strncmp(a.data.data(), b.data.data(), a.data.size()) < 0; });

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

TEST(BTree, PageTypes)
{
  Page<BTreeHeader> root(PageType::Root);
  EXPECT_EQ(PageType::Root, root.header()->common.type);
}

TEST(BTree, SearchGetLeaf)
{
  /* Create this tree:
   *    [3,_]
   *   /   \
   * [2]-->[3,4]
   * Search for 3
   */
  std::stringstream mockStream;
  Pager pager(mockStream);

  PageId rootId{};
  auto &rootPage = pager.fromNextFree<BTreeHeader>(PageType::Interior, &rootId);
  InteriorNode root{rootPage};
  InteriorCell top = InteriorCell(static_cast<u32>(3));

  // create the leaf nodes
  PageId leftId{};
  auto &leftPage = pager.fromNextFree<BTreeHeader>(PageType::Leaf, &leftId);
  LeafNode left = LeafNode(leftPage);
  PageId rightId{};
  auto &rightPage = pager.fromNextFree<BTreeHeader>(PageType::Leaf, &rightId);
  LeafNode right{rightPage};

  // point the top node keys to the leaf nodes
  top.leftChild = leftId;
  InteriorCell topEnd = InteriorCell<u32>::End();
  topEnd.leftChild = rightId;

  root.page.header()->slots.insertCell(top);
  root.page.header()->slots.insertCell(topEnd);

  left.page.header()->parent = rootId;
  left.page.header()->slots.insertCell(LeafCell(static_cast<u32>(2)));
  left.setSibling(rightId);
  right.page.header()->parent = rootId;
  right.page.header()->slots.insertCell(LeafCell(static_cast<u32>(3)));
  right.page.header()->slots.insertCell(LeafCell(static_cast<u32>(4)));

  {
    // search for 4, should return the `right` leaf node.
    // It will not point directly to 4, or confirm that 4 exists
    const Page<BTreeHeader> &res = root.searchGetLeaf(pager, static_cast<u32>(4));
    ASSERT_TRUE(res.header()->isLeaf());
    EXPECT_EQ(rootId, res.header()->parent);

    // make sure its the right leaf node and the cells are in the correct order
    auto it = res.header()->slots.begin();
    Slot s1 = *it;
    NodeCell c1 = NodeCell<u32>(res.header()->slots, s1);
    // 3 comes first
    EXPECT_EQ(sizeof(u32), c1.payloadSize);
    EXPECT_EQ(3, c1.getPayload());
    ++it;
    Slot s2 = *it;
    NodeCell c2 = NodeCell<u32>(res.header()->slots, s2);
    EXPECT_EQ(sizeof(u32), c2.payloadSize);
    EXPECT_EQ(4, c2.getPayload());
  }

  {
    // search for 2, should return the `left` leaf node.
    Page<BTreeHeader> &res = root.searchGetLeaf(pager, static_cast<u32>(2));
    ASSERT_TRUE(res.header()->isLeaf());
    EXPECT_EQ(rootId, res.header()->parent);

    // make sure its the left leaf node and the cells are in the correct order
    auto it = res.header()->slots.begin();
    Slot s1 = *it;
    NodeCell c1 = NodeCell<u32>(res.header()->slots, s1);
    EXPECT_EQ(sizeof(u32), c1.payloadSize);
    EXPECT_EQ(2, c1.getPayload());
    ++it;
    // no more cells
    EXPECT_EQ(res.header()->slots.end(), it);

    // check the sibling link is there still
    LeafNode leafRes {res};
    EXPECT_EQ(rightId, leafRes.getSibling());
  }
}

TEST(BTree, SearchInLeafGetSlot)
{
  std::stringstream mockStream;
  Pager pager(mockStream);
  LeafNode l{pager.fromNextFree<BTreeHeader>(PageType::Leaf)};
  l.page.header()->slots.insertCell(LeafCell(static_cast<u32>(3)));
  l.page.header()->slots.insertCell(LeafCell(static_cast<u32>(4)));

  LeafCell q = LeafCell(static_cast<u32>(4));
  auto res = l.searchGetSlot(q);
  ASSERT_TRUE(res.has_value());
  auto [pSlot, pCell] = res.value();
  ASSERT_TRUE(nullptr != pSlot);
  ASSERT_TRUE(nullptr != pCell);
  EXPECT_EQ(sizeof(LeafCell<u32>), pSlot->cellSize);
}

TEST(BTree, SplitNode)
{
  std::stringstream mockStream;
  Pager pager(mockStream);
  PageId pageId{};
  InteriorNode n1{pager.fromNextFree<BTreeHeader>(PageType::Interior, &pageId)};
  n1.page.header()->slots.insertCell(InteriorCell(static_cast<u32>(1)));
  n1.page.header()->slots.insertCell(InteriorCell(static_cast<u32>(2)));
  n1.page.header()->slots.insertCell(InteriorCell(static_cast<u32>(3)));
  n1.page.header()->slots.insertCell(InteriorCell(static_cast<u32>(4)));

  pager.flushPage(pageId, n1.page.as_ref<CommonHeader>()); // write it to disk now

  auto &n2 = n1.page.header()->split(pager);

  // current node takes the upper half (3, 4)
  // the new node takes the lower half (1,2)
  // this is so that inserting can reuse the original node's parent link
  // and have the new value inserted

  EXPECT_EQ(2, n1.page.header()->slots.entryCount());
  {
    auto it1 = n1.page.header()->slots.begin();
    {
      Slot &s = *it1;
      InteriorCell ic = *reinterpret_cast<const InteriorCell<u32> *>(
          n1.page.header()->slots.readCell(s.cellOffset));
      EXPECT_EQ(3, ic.cell.getPayload());
    }
    ++it1;
    {
      Slot &s = *it1;
      InteriorCell ic = *reinterpret_cast<const InteriorCell<u32> *>(
          n1.page.header()->slots.readCell(s.cellOffset));
      EXPECT_EQ(4, ic.cell.getPayload());
    }
    ++it1;
    EXPECT_EQ(n1.page.header()->slots.end(), it1);
  }

  // the new node's values
  EXPECT_EQ(2, n2.header()->slots.entryCount());
  {
    auto it2 = n2.header()->slots.begin();
    {
      Slot &s = *it2;
      const InteriorCell<u32> *ic =
          reinterpret_cast<const InteriorCell<u32> *>(n2.header()->slots.readCell(s.cellOffset));
      EXPECT_EQ(1, ic->cell.getPayload());
    }
    ++it2;
    {
      Slot &s = *it2;
      InteriorCell ic =
          *reinterpret_cast<const InteriorCell<u32> *>(n2.header()->slots.readCell(s.cellOffset));
      EXPECT_EQ(2, ic.cell.getPayload());
    }
    ++it2;
    EXPECT_EQ(n2.header()->slots.end(), it2);
  }
}

/* test that adding the maximum size cell fills up the btree node */
TEST(BTree, NodeCanFit_BTREE_ORDER_Cells)
{
  Page<BTreeHeader> node;

  struct MaxCell
  {
    std::size_t i;
    std::array<std::byte, MAX_CELL_SIZE - sizeof(i)> data;
    MaxCell(std::size_t i) : i(i) { data.fill(static_cast<std::byte>(0)); }
  };
  static_assert(sizeof(MaxCell) == MAX_CELL_SIZE,
                "sizeof(MaxCell) struct should be the maximum cell size");

  auto comparitor = std::function([](const MaxCell &a, const MaxCell &b) { return a.i < b.i; });
  for (std::size_t i{0}; i < BTREE_ORDER; ++i)
  {
    node.header()->slots.insertCell(MaxCell(i), comparitor);
  }
  EXPECT_EQ(BTREE_ORDER, node.header()->slots.entryCount());

  std::size_t numSlots{0};
  for (const auto &slot : node.header()->slots)
  {
    auto cell = reinterpret_cast<MaxCell *>(node.header()->slots.getCell(slot.cellOffset));
    EXPECT_EQ(numSlots, cell->i);
    ++numSlots;
  }
  EXPECT_EQ(BTREE_ORDER, numSlots);
  // there should not be enough space to fit another cell+slot
  EXPECT_GT(MAX_CELL_SIZE + sizeof(Slot), node.header()->slots.freeLength);
}

TEST(BTree, LeafNodeFromExistingPage)
{
  Page<BTreeHeader> p1(PageType::Interior);
  LeafNode leaf(p1);
  EXPECT_EQ(leaf.page.header()->common.type, p1.header()->common.type);

  p1.header()->common.type = PageType::Leaf;

  EXPECT_EQ(leaf.page.header()->common.type, p1.header()->common.type);
}

TEST(BTree, SplitRoot)
{

  std::stringstream mockStream;
  Pager pager(mockStream);
  PageId originalRootId{};
  LeafNode originalRoot(pager.fromNextFree<BTreeHeader>(PageType::Leaf, &originalRootId));

  ASSERT_TRUE(originalRoot.page.header()->isRoot());
  ASSERT_TRUE(originalRoot.page.header()->isLeaf());
  ASSERT_EQ(0, originalRoot.page.header()->slots.entryCount());

  for (std::size_t i{}; i < BTREE_ORDER; ++i)
  {
    leafInsert<u32>(pager, originalRootId, originalRoot.page, static_cast<u32>(123));
  }

  EXPECT_TRUE(originalRoot.page.header()->isRoot());
  EXPECT_TRUE(originalRoot.page.header()->isLeaf());
  EXPECT_EQ(BTREE_ORDER, originalRoot.page.header()->slots.entryCount());

  // next insert will cause a split
  leafInsert<u32>(pager, originalRootId, originalRoot.page, static_cast<u32>(123));
  ASSERT_FALSE(originalRoot.page.header()->isRoot());
  ASSERT_TRUE(originalRoot.page.header()->isLeaf());

  const auto &newRoot = pager.getPage<BTreeHeader>(originalRoot.page.header()->parent);
  EXPECT_TRUE(newRoot.header()->isRoot());
  EXPECT_FALSE(newRoot.header()->isLeaf());

  // new root contains the key for the new node and an end node pointing to the original node
  EXPECT_EQ(2, newRoot.header()->slots.entryCount());

  auto rootIt = newRoot.header()->slots.begin();
  {
    Slot s1 = *rootIt;
    InteriorCell c1 = InteriorCell<u32>::fromSlot(newRoot.header()->slots, s1);
    EXPECT_EQ(123, c1.cell.getPayload());
    // split should cause the first cell to point to the new node.
    // we don't know the Id of that though.
    EXPECT_NE(0, c1.leftChild);
  }

  ++rootIt;
  EXPECT_NE(newRoot.header()->slots.end(), rootIt);
  {
    Slot s2 = *rootIt;
    InteriorCell c2 = InteriorCell<u32>::fromSlot(newRoot.header()->slots, s2);
    EXPECT_TRUE(c2.isEnd());
    // the split should have made end point to original node
    EXPECT_EQ(originalRootId, c2.leftChild);
  }
}
