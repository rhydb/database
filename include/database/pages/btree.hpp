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

  friend bool operator==(const Slot &a, const Slot &b)
  {
    return a.cellOffset == b.cellOffset && a.cellSize == b.cellSize;
  }
};

struct InteriorCell;
struct NodeCell;
using LeafCell = NodeCell;

// not to be used independently
// used for page types to support slots
struct SlotHeader
{
  struct iterator;
  iterator begin();
  iterator end();

  u16 freeStart = 0; // offset from end of header
  u16 freeLength;    // marked as free if both 0

  bool isEmpty() const noexcept { return freeStart == 0; }
  bool isFree() const noexcept { return freeStart == 0 && freeLength == 0; }

  // data is the region of the page that is used to store slots and cells
  // it does not include the slot header itself or any other data, just slots and cells
  explicit SlotHeader(std::span<std::byte> data) : buf(data) { freeLength = buf.size(); }

  bool isSlotOutOfBounds(SlotNum slotNumber) { return slotNumber * sizeof(Slot) > freeStart; }
  Slot *getSlot(SlotNum slotNumber);
  // get a slot and its cell using its number
  void *getSlotAndCell(SlotNum slotNumber, Slot *retSlot);

  inline std::byte *getCell(u16 offset) { return buf.data() + offset; }

  // const version of getCell
  inline const std::byte *readCell(u16 offset) const { return buf.data() + offset; }

  // delete the slot from the slot array.
  void deleteSlot(SlotNum slotNum)
  {
    Slot *s = getSlot(slotNum);
    deleteSlot(&s);
  }

  // add data to a new cel and create its slot in the sorted position
  // this creates a new cell in the page and assigns a slot to it.
  // the slot will be in its sorted position and may reuse a free slot if one is present
  // returns the slot the cell was inserted into
  template <typename Cell>
  Slot *insertCell(const Cell &cell,
                   std::function<bool(const Cell &a, const Cell &b)> comparator = std::less<Cell>{},
                   u16 *retSlotNumber = nullptr)
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
    s->cellOffset = allocNextCell(sizeof(Cell));

    std::byte *cellDst = getCell(s->cellOffset);

    std::memcpy(cellDst, &cell, sizeof(Cell));

    return s;
  }

  template <typename P> Slot *insertCell(const InteriorCell &cell, u16 *retSlotNumber = nullptr);
  template <typename P> Slot *insertCell(const LeafCell &cell, u16 *retSlotNumber = nullptr);

  // create a cell of size cellSize from the free space
  u16 allocNextCell(u16 cellSize);
  // create a new slot in the free space
  Slot *createNextSlot(u16 cellSize, u16 *retSlotNum = nullptr);
  // create a new slot and new cell in the free space
  // assigns the slot to the newly created cell
  void *createNextSlotWithCell(u16 cellSize, SlotNum *retSlotNumber);

private:
  // TODO: how can we avoid storing this in the struct, so that sizeof returns just the header?
  // this is important as it affects the other headers
  std::span<std::byte> buf;

  SlotNum slotNumber(const Slot *s)
  {
    // how many slots between the header end and s
    assert(reinterpret_cast<intptr_t>(s) >= reinterpret_cast<intptr_t>(buf.data()) &&
           reinterpret_cast<intptr_t>(s) <= reinterpret_cast<intptr_t>(buf.data() + buf.size()) &&
           "Slot pointer out of bounds to retrieve slot number");
    return (reinterpret_cast<intptr_t>(s) - reinterpret_cast<intptr_t>(buf.data())) / sizeof(Slot);
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

struct SlotHeader::iterator
{
  using difference_type = SlotNum;
  using pointer = const Slot *;
  using reference = const Slot &;

  explicit iterator(SlotHeader *slots) noexcept;
  iterator() : m_slots(nullptr), m_isEnd(true), m_current() {}

  reference operator*() const noexcept { return m_current; }
  pointer operator->() const noexcept { return &m_current; }
  iterator &operator++();
  iterator operator++(int);
  friend bool operator==(const iterator &a, const iterator &b)
  {
    if (a.m_isEnd && b.m_isEnd)
      return true;
    return a.m_isEnd == b.m_isEnd && a.m_slots == b.m_slots && a.m_current == b.m_current;
  }
  friend bool operator!=(const iterator &a, const iterator &b) { return !(a == b); }

private:
  SlotHeader *m_slots = nullptr;
  bool m_isEnd = false;
  Slot m_current;
  SlotNum m_currentNum = 0;
};

static constexpr std::size_t MAX_CELL_PAYLOAD = 32;

union CellPayload
{
  struct
  {
    std::array<std::byte, MAX_CELL_PAYLOAD - sizeof(PageId)> payloadStart;
    PageId overflow;
  } large;

  std::array<std::byte, MAX_CELL_PAYLOAD> small;

  u32 smallPayloadSize(u32 payloadSize) const
  {
    return std::min(payloadSize, static_cast<u32>(MAX_CELL_PAYLOAD));
  }
};

/* Generic size+payload cell for any node. Every cell in a leaf node is just this */
struct NodeCell
{
  u32 payloadSize;
  CellPayload payload;

  template <typename T> explicit NodeCell(T data)
  {
    payloadSize = sizeof(T);
    // TODO: overflow
    std::memset(payload.small.data(), 0, payload.small.size());
    std::memcpy(payload.small.data(), &data, payloadSize);
  }

  NodeCell(const SlotHeader &sh, const Slot &s)
  {
    // read the structure of the cell from the slot
    const std::byte *pCell = sh.readCell(s.cellOffset);
    payloadSize = *reinterpret_cast<const u32 *>(pCell);
    // TODO overflow. this might be fine, as NodeCell is just a wrapper around what goes into the
    // slots
    payload = *reinterpret_cast<const CellPayload *>(pCell + sizeof(payloadSize));
    // if we want to read the entire payload, this is where we could follow `payload.overflow`
  }

  template <typename T> T getPayload() const noexcept
  {
    // TODO: overflow
    return *reinterpret_cast<const T *>(payload.small.data());
  }

  /* calculate the size of the struct. the payload size might be larger than the max payload,
   * so return the total size of the struct. If it is smaller, return the minimum size of the
   * struct and plus the payload size */
  u32 cellSize() const noexcept
  {
    if (payloadSize > MAX_CELL_PAYLOAD)
    {
      return sizeof(NodeCell);
    }

    return sizeof(payloadSize) + payloadSize;
  }
};

static_assert(sizeof(CellPayload::large) == MAX_CELL_PAYLOAD,
              "Large payload data must fit into MAX_CELL_PAYLOAD");

/* The cells used in interior nodes. They store the a pointer to their left child in the btree along
 * with the payload data itself for the key */
struct InteriorCell
{
  PageId leftChild = 0;
  NodeCell cell;

  template <typename T> explicit InteriorCell(T data) : cell(data) {}

  bool isEnd() const noexcept { return cell.payloadSize == 0; }

  static InteriorCell End() noexcept
  {
    InteriorCell c(0);
    c.leftChild = 0;
    c.cell.payloadSize = 0;
    return c;
  }
};

struct BTreeHeader
{
  CommonHeader common;
  PageId parent = 0;
  SlotHeader slots;

  BTreeHeader(std::array<std::byte, PAGE_SIZE> &array)
      : slots(std::span<std::byte>(array.data() + sizeof(BTreeHeader),
                                   array.size() - sizeof(BTreeHeader)))
  {
  }

  bool isRoot() const noexcept { return parent == 0; }
  bool isLeaf() const noexcept { return common.type == PageType::Leaf; }
};
static_assert(offsetof(BTreeHeader, common) == 0, "Common header must be at offset 0");
/* we determine the btree order from the max cell size. this will need to be determined presumably
 * through benchmarking. Since the payload of the cells is not known, here we are allowing a 32 byte
 * payload. MAX_CELL_SIZE is the maximum total cell size (i.e. payload size + the payload itself)
 * within the slotted page's cell. A cell larger than this size is then extended into the overflow
 * page */
static constexpr std::size_t MAX_CELL_SIZE =
    std::max(sizeof(NodeCell), sizeof(InteriorCell)) + MAX_CELL_PAYLOAD;
static constexpr std::size_t BTREE_ORDER =
    (PAGE_SIZE - sizeof(BTreeHeader)) / (MAX_CELL_SIZE + sizeof(Slot));
static_assert(BTREE_ORDER > 0, "BTree order must be at least 1");

struct InteriorNode
{
  Page<BTreeHeader> page = Page<BTreeHeader>(Interior);

  /* find the leaf node a cell value would be located in.
   * following the leaf linked list is not needed to find the existence of teh value */
  template <typename V> Page<BTreeHeader> &searchGetLeaf(Pager &pager, const V &Q)
  {
    // find the node to follow down
    // TODO: maybe a binary search here to find the node

    Page<BTreeHeader> &currentPage = this->page;

    while (!currentPage.header()->isLeaf())
    {
      for (const Slot &s : currentPage.header()->slots)
      {
        // get the interior cell for this slot
        assert(s.cellSize == sizeof(InteriorCell) &&
               "Interior search cell should be size of Interior");
        const InteriorCell interior =
            *reinterpret_cast<InteriorCell *>(currentPage.header()->slots.getCell(s.cellOffset));
        assert(interior.leftChild != 0 &&
               "Non leaf node cannot have leaf cell. Tree must be unbalanced");
        if (interior.isEnd())
        {
          // end cell of this node, follow it down
          currentPage = pager.getPage<BTreeHeader>(interior.leftChild);
          break;
        }

        // TODO: overflow
        assert(interior.cell.payloadSize == sizeof(V) &&
               "Interior cell payload should be size of search type");
        // get the payload for the cell - the search value in this node
        const V value = *reinterpret_cast<const V *>(interior.cell.payload.small.data());
        if (Q < value)
        {
          // follow the slot to the next node
          currentPage = pager.getPage<BTreeHeader>(interior.leftChild);
          break;
        }
      }
    }

    return currentPage;
  }
};

struct LeafNode
{
  Page<BTreeHeader> page;

  struct Reserved
  {
    PageId sibling;
  };

  LeafNode() : page(PageType::Leaf)
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

template <typename P> Slot *SlotHeader::insertCell(const InteriorCell &cell, u16 *retSlotNumber)
{
  // TODO: can we use the operator< for InteriorCell instead?
  // since they both have to be the same payload type
  const auto interiorComp = std::function(
      [](const InteriorCell &a, const InteriorCell &b)
      {
        // end slots always go at the end
        // a < b == true
        if (a.isEnd())
        {
          return false;
        }
        if (b.isEnd())
        {
          return true;
        }
        return a.cell.getPayload<P>() < b.cell.getPayload<P>();
      });

  return insertCell(cell, interiorComp, retSlotNumber);
}

template <typename P> Slot *SlotHeader::insertCell(const LeafCell &cell, u16 *retSlotNumber)
{
  // TODO: can we use the operator< for LeafCell instead?
  // since they both have to be the same payload type
  const auto leafComp = std::function([](const LeafCell &a, const LeafCell &b)
                                      { return a.getPayload<P>() < b.getPayload<P>(); });

  return insertCell(cell, leafComp, retSlotNumber);
}
