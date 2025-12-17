#include "database/pages/btree.hpp"

Slot *SlotHeader::getSlot(SlotNum slotNumber)
{
  // go to the end of this header struct, then to the specified slot
  // allow getting the slot on the boundary of freeStart as that's how we get new cells
  if (isSlotOutOfBounds(slotNumber))
  {
    throw std::out_of_range("Slot number out of bounds");
  }
  return reinterpret_cast<Slot *>(buf.data() + slotNumber * sizeof(Slot));
}

void *SlotHeader::getSlotAndCell(SlotNum slotNumber, Slot *retSlot)
{
  Slot *s = getSlot(slotNumber);
  if (retSlot != nullptr)
  {
    *retSlot = *s;
  }
  return getCell(s->cellOffset);
}

u16 SlotHeader::allocNextCell(u16 cellSize)
{
  assert(freeLength >= cellSize && "Not enough room in page for new cell");

  const u16 cellOffset = freeStart + freeLength - cellSize;
  freeLength -= cellSize;

  return cellOffset;
}

Slot *SlotHeader::createNextSlot(u16 cellSize, u16 *retSlotNum)
{
  assert(freeLength >= cellSize + sizeof(Slot) && "Not enough room in page for new cell&slot");
  Slot *slot = reinterpret_cast<Slot *>(buf.data() + freeStart);

  assert(cellSize > 0 && "New slot cannot have cellSize of 0 to not be marked free");
  slot->cellSize = cellSize;

  // shrinks from both sides
  freeStart += sizeof(Slot);
  freeLength -= sizeof(Slot);

  if (retSlotNum != nullptr)
  {
    *retSlotNum = slotNumber(slot);
  }

  return slot;
}

void *SlotHeader::createNextSlotWithCell(u16 cellSize, SlotNum *retSlotNumber)
{
  assert(freeLength >= cellSize + sizeof(Slot) && "Not enough room in page for new cell&slot");
  char *headerEnd = reinterpret_cast<char *>(this + 1);
  Slot *slot = reinterpret_cast<Slot *>(headerEnd + freeStart);
  slot->cellSize = cellSize;

  freeStart += sizeof(Slot);
  slot->cellOffset = freeStart + freeLength - cellSize;

  // shrinks from both sides
  freeLength -= slot->cellSize;
  freeLength -= sizeof(Slot);

  if (retSlotNumber != nullptr)
  {
    *retSlotNumber = (reinterpret_cast<intptr_t>(slot) - reinterpret_cast<intptr_t>(headerEnd)) / sizeof(Slot);
  }

  return headerEnd + slot->cellOffset;
}

std::optional<std::pair<Slot*,LeafCell*>> LeafNode::searchGetSlot(const LeafCell &Q)
{
  // TODO: binary search?
  for (Slot &s : page.header()->slots)
  {
    // TODO: overflow
    LeafCell *c = reinterpret_cast<LeafCell*>(page.header()->slots.getCell(s.cellOffset));
    // TODO: ordering
    if (Q.payload.small == c->payload.small)
    {
      // TODO: overflow
      return std::pair(&s, c);
    }
  }

  return std::nullopt;
}

SlotHeader::iterator SlotHeader::begin() { return iterator(this); }
SlotHeader::iterator SlotHeader::end() { return iterator(); }

SlotHeader::iterator::iterator(SlotHeader *slots) noexcept
    : m_slots(slots)
{
  if (slots == nullptr || slots->isEmpty() || slots->isSlotOutOfBounds(0))
  {
    m_isEnd = true;
    return;
  }

  m_current = slots->getSlot(0);
}

SlotHeader::iterator &SlotHeader::iterator::operator++()
{
  if (m_isEnd || m_slots == nullptr)
    return *this;

  m_currentNum++;
  if (m_currentNum >= (m_slots->freeStart / sizeof(Slot)))
  {
    m_isEnd = true;
  }
  else
  {
    m_current = m_slots->getSlot(m_currentNum);
  }

  return *this;
}

SlotHeader::iterator SlotHeader::iterator::operator++(int)
{
  iterator tmp = *this;
  ++(*this);
  return tmp;
}
