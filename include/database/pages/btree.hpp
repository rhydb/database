#pragma once

#include "page_header.hpp"
#include "../pager.hpp"

#include <cstring>
#include <cassert>
#include <span>
#include <functional>
#include <memory>

// we use the slot numbers in the table b tree for row ids
// this is used in indexes to then find the row data
using SlotNum = u16;
struct Slot
{
  u16 cellOffset; // from the end of the header
  u16 cellSize;   // mark as 0 for free
};

// not to be used independently
// used for page types to support slots
struct SlotHeader
{
  u16 freeStart = 0; // offset from end of header
  u16 freeLength;    // marked as free if both 0

  // data is the region of the page that is used to store slots and cells
  // it does not include the slot header itself or any other data, just slots and cells
  explicit SlotHeader(std::span<std::byte> data)
      : m_data(data) {
        freeLength = m_data.size();
      }

  Slot *getSlot(SlotNum slotNumber);
  // get a slot and its cell using its number
  void *getSlotAndCell(SlotNum slotNumber, Slot *retSlot);

  inline void *getCell(u16 offset)
  {
    return m_data.data() + offset;
  }

  // delete the slot from the slot array.
  void deleteSlot(SlotNum slotNum)
  {
    Slot *s = getSlot(slotNum);
    deleteSlot(&s);
  }

  // add data to a new cel and create its slot in the sorted position
  // this creates a new cell in the page and assigns a slot to it.
  // the slot will be in its sorted position and may reuse a free slot if one is present
  template <typename Cell>
  Slot *insertCell(const Cell &cell, std::function<bool(const Cell &a, const Cell &b)> comparator = std::less<Cell>{}, u16 *retSlotNumber = nullptr)
  {
    int l = 0;
    int r = freeStart / sizeof(Slot) - 1;

    while (l <= r)
    {
      SlotNum mid = l + (r - l) / 2;
      const Cell *midCell = reinterpret_cast<Cell *>(getSlotAndCell(mid, nullptr));

      if (comparator(cell, *midCell))
      {
        // cell < midCell
        r = mid - 1;
      }
      else
      {
        l = mid + 1;
      }
    }

    Slot *s = insertSlot(l);

    if (retSlotNumber != nullptr)
    {
      *retSlotNumber = l;
    }

    s->cellSize = sizeof(Cell);
    s->cellOffset = createNextCell(sizeof(Cell));

    void *cellDst = getCell(s->cellOffset);

    std::memcpy(cellDst, &cell, sizeof(Cell));

    return s;
  }

  // create a cell of size cellSize from the free space
  u16 createNextCell(u16 cellSize);
  // create a new slot in the free space
  Slot *createNextSlot(u16 cellSize, u16 *retSlotNum = nullptr);
  // create a new slot and new cell in the free space
  // assigns the slot to the newly created cell
  void *createNextSlotWithCell(u16 cellSize, SlotNum *retSlotNumber);

private:
  std::span<std::byte> m_data;

  SlotNum slotNumber(const Slot *s)
  {
    // how many slots between the header end and s
    assert(
        reinterpret_cast<intptr_t>(s) >= reinterpret_cast<intptr_t>(m_data.data())
        && reinterpret_cast<intptr_t>(s) <= reinterpret_cast<intptr_t>(m_data.data() + m_data.size())
        && "Slot pointer out of bounds to retrieve slot number");
    return (reinterpret_cast<intptr_t>(s) - reinterpret_cast<intptr_t>(m_data.data())) / sizeof(Slot);
  }

  Slot *insertSlot(SlotNum slotNum)
  {
    Slot *s = getSlot(slotNum);
    const size_t bytes = freeStart - (slotNum * sizeof(Slot));
    std::memmove(s + 1, s, bytes);
    freeLength -= sizeof(Slot);
    freeStart += sizeof(Slot);
    return s;
  }

  // delete the slot from the slot array.
  // sets s to nullptr.
  void deleteSlot(Slot **ppSlot)
  {
    /*
     * |0,1,2,3,4|
     *
     */
    const size_t bytes = freeStart - ((slotNumber(*ppSlot) + 1) * sizeof(Slot));
    std::memmove(*ppSlot, (*ppSlot) + 1, bytes);
    // opposite to insertSlot :)
    freeStart -= sizeof(Slot);
    freeLength += sizeof(Slot);
    *ppSlot = nullptr;
  }

};

struct LeafCell
{
  struct Cell
  {
    u32 payloadSize;
    union
    {
      u32 key;
      PageId leaf;
    };
  } cell;
  std::unique_ptr<char[]> data;
};

struct InteriorCell
{
  PageId leftChild;
  u32 key;
};

struct BTreeHeader
{
  CommonHeader common;
  PageId parent = 0;
  SlotHeader slots;

  BTreeHeader(std::array<std::byte, PAGE_SIZE> array) : slots(std::span<std::byte>(array.data(), array.size())) {}
};
static_assert(offsetof(BTreeHeader, common) == 0, "Common header must be at offset 0");
static constexpr std::size_t MAX_CELL_SIZE = std::max(sizeof(LeafCell), sizeof(InteriorCell)) + 32;
static constexpr std::size_t BTREE_ORDER = (PAGE_SIZE - sizeof(BTreeHeader)) / (MAX_CELL_SIZE + sizeof(Slot));
static_assert(BTREE_ORDER > 0, "BTree order must be at least 1");


struct LeafNode
{
  Page<BTreeHeader> page;

  struct Reserved
  {
    PageId sibling;
  };

  LeafNode()
      : page(PageType::Leaf)
  {
    BTreeHeader *header = page.header();
    header->slots.freeStart = 0;
    header->slots.freeLength = PAGE_SIZE - header->slots.freeStart - sizeof(Reserved);

    setSibling(0);
  }

  PageId getSibling()
  {
    Reserved *r = reserved();
    return r->sibling;
  }

  void setSibling(PageId sibling)
  {
    Reserved *r = reserved();
    r->sibling = sibling;
  }

private:
  inline Reserved *reserved()
  {
    return reinterpret_cast<Reserved *>(page.buf.data() + page.buf.size() - sizeof(Reserved));
  }
};
