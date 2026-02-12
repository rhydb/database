#include "database/pages/btree.hpp"

Page<BTreeHeader> &BTreeHeader::split(Pager &pager, PageId *retPageId)
{
  // these are 1-indexed
  const auto K = slots.entryCount(); // BTREE_ORDER;
  const decltype(K) half = (K / 2) + (K & 1);

  Page<BTreeHeader> &newPage = pager.fromNextFree<BTreeHeader>(this->common.type, retPageId);
  newPage.header()->parent = this->parent;

  // move the bottom half of this node to the new node
  for (u16 i{0}; i < half; ++i)
  {
    const Slot *s1 = slots.getSlot(0); // always read 0 as we pop it afterwards
    const std::byte *c1 = slots.readCell(s1->cellOffset);
    SlotNum s2Num = 0;

    std::byte *c2 = reinterpret_cast<std::byte *>(
        newPage.header()->slots.createNextSlotWithCell(s1->cellSize, &s2Num));
    std::memcpy(c2, c1, s1->cellSize);
    // TODO: we are doing memmove repeatedly here :(
    slots.deleteSlot(static_cast<SlotNum>(0));
  }

  return newPage;
}
