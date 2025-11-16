#pragma once

#include "page_header.hpp"
#include "../pager.hpp"

#include <algorithm>
#include <cstring>
#include <cassert>
#include <span>
#include <functional>

// we use the slot numbers in the table b tree for row ids
// this is used in indexes to then find the row data
using SlotNum = u16;
struct Slot
{
  u16 cellOffset; // from the end of the header
  /* the size of the cell within the slotted page.
   * the total size should then be stored in the cell */
  u16 cellSize;
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

static constexpr std::size_t MAX_CELL_PAYLOAD = 32;

struct LeafCell
{
    u32 payloadSize;

    union LeafData {
      struct {
        std::array<std::byte, MAX_CELL_PAYLOAD - sizeof(PageId)> payloadStart;
        PageId overflow;
      } largePayload;

      std::array<std::byte, MAX_CELL_PAYLOAD> smallPayload;
    } data;

    u32 smallPayloadSize() const {
      return std::min(payloadSize, static_cast<u32>(MAX_CELL_PAYLOAD));
    }

    /* calculate the size of the struct. the payload size might be larger than the max payload,
     * so return the total size of the struct. If it is smaller, return the minimum size of the
     * struct and plus the payload size */
    u32 cellSize() const noexcept {
      if (payloadSize > MAX_CELL_PAYLOAD) {
        return sizeof(LeafCell);
      }

      return sizeof(payloadSize) + payloadSize;
    }
};

static_assert(sizeof(LeafCell::LeafData::largePayload) == MAX_CELL_PAYLOAD, "Large payload data must fit into MAX_CELL_PAYLOAD");

struct InteriorCell
{
  PageId leftChild;
  std::array<std::byte, 4> key;
};

struct BTreeHeader
{
  CommonHeader common;
  PageId parent = 0;
  SlotHeader slots;

  BTreeHeader(std::array<std::byte, PAGE_SIZE> array) : slots(std::span<std::byte>(array.data(), array.size())) {}
};
static_assert(offsetof(BTreeHeader, common) == 0, "Common header must be at offset 0");
/* we determine the btree order from the max cell size. this will need to be determined presumably
 * through benchmarking. Since the payload of the cells is not known, here we are allowing a 32 byte
 * payload. MAX_CELL_SIZE is the maximum total cell size (i.e. payload size + the payload itself)
 * within the slotted page's cell. A cell larger than this size is then extended into the overflow page */
static constexpr std::size_t MAX_CELL_SIZE = std::max(sizeof(LeafCell), sizeof(InteriorCell)) + MAX_CELL_PAYLOAD;
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
