#include "database/pages/btree.hpp"

Slot *SlotHeader::getSlot(SlotNum slotNumber)
{
  // go to the end of this header struct, then to the specified slot
  // allow getting the slot on the boundary of freeStart as that's how we get new cells
  if (slotNumber * sizeof(Slot) > freeStart)
  {
    throw std::out_of_range("Slot number out of bounds");
  }
  return reinterpret_cast<Slot *>(m_data.data() + slotNumber * sizeof(Slot));
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
