#pragma once

#include "page_header.hpp"
#include "../pager.hpp"

#include <any>
#include <algorithm>
#include <cassert>
#include <span>
#include <functional>
#include <optional>

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

template <typename T> struct InteriorCell;
template <typename T> struct NodeCell;
template <typename T> using LeafCell = NodeCell<T>;

// not to be used independently
// used for page types to support slots
// data is the region of the page that is used to store slots and cells
// it does not include the slot header itself or any other data, just slots and cells
template <std::size_t bufsize> struct SlotHeader
{
  struct iterator;
  iterator begin() { return iterator(this); }
  iterator end() { return iterator(); }

  u16 freeStart = 0;        // offset from end of header
  u16 freeLength = bufsize; // marked as free if both 0

  bool isEmpty() const noexcept { return freeStart == 0; }
  bool isFree() const noexcept { return freeStart == 0 && freeLength == 0; }
  u16 entryCount() const noexcept { return freeStart / sizeof(Slot); }

  bool isSlotOutOfBounds(SlotNum slotNumber) { return slotNumber * sizeof(Slot) > freeStart; }
  Slot *getSlot(SlotNum slotNumber)
  {
    if (isSlotOutOfBounds(slotNumber))
    {
      throw std::out_of_range("Slot number out of bounds");
    }
    return reinterpret_cast<Slot *>(buf().data() + slotNumber * sizeof(Slot));
  }
  // get a slot and its cell using its number
  void *getSlotAndCell(SlotNum slotNumber, Slot *retSlot)
  {
    Slot *s = getSlot(slotNumber);
    if (retSlot != nullptr)
    {
      *retSlot = *s;
    }
    return getCell(s->cellOffset);
  }

  inline std::byte *getCell(u16 offset) { return buf().data() + offset; }

  // const version of getCell
  inline const std::byte *readCell(u16 offset) const { return buf().data() + offset; }

  // delete the slot from the slot array.
  void deleteSlot(SlotNum slotNum)
  {
    Slot *s = getSlot(slotNum);
    deleteSlot(&s);
  }

  // add data to a new cell and create its slot in the sorted position
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

  template <typename P> Slot *insertCell(const InteriorCell<P> &cell, u16 *retSlotNumber = nullptr);
  template <typename P> Slot *insertCell(const LeafCell<P> &cell, u16 *retSlotNumber = nullptr);

  // create a cell of size cellSize from the free space
  u16 allocNextCell(u16 cellSize)
  {
    assert(freeLength >= cellSize && "Not enough room in page for new cell");

    const u16 cellOffset = freeStart + freeLength - cellSize;
    freeLength -= cellSize;

    return cellOffset;
  }
  /* allocate the next slot and set its cellSize
   * returns the new Slot created and optionally its slot number */
  Slot *createNextSlot(u16 cellSize, u16 *retSlotNum = nullptr)
  {
    assert(freeLength >= cellSize + sizeof(Slot) && "Not enough room in page for new cell&slot");
    Slot *slot = reinterpret_cast<Slot *>(buf().data() + freeStart);

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

  /* like createNextSlot, but also allocates its cell, and sets the slot's cell offset.
   * returns the newly created cell and optionally the slot number */
  [[nodiscard]] void *createNextSlotWithCell(u16 cellSize, SlotNum *retSlotNumber)
  {
    assert(freeLength >= cellSize + sizeof(Slot) && "Not enough room in page for new cell&slot");
    Slot *s = createNextSlot(cellSize, retSlotNumber);
    s->cellOffset = allocNextCell(cellSize);
    return getCell(s->cellOffset);
  }

private:
  // TODO: LeafNodes store their reserved data at the end of the buffer, so the bufsize here is
  // incorrect for them. The memory is valid, but points into the reserved data.

  std::span<std::byte> buf() noexcept
  {
    return std::span<std::byte>(reinterpret_cast<std::byte *>(this + 1), bufsize);
  }

  std::span<const std::byte> buf() const noexcept
  {
    return std::span<const std::byte>(reinterpret_cast<const std::byte *>(this + 1), bufsize);
  }

  SlotNum slotNumber(const Slot *s)
  {
    // how many slots between the header end and s
    assert(reinterpret_cast<intptr_t>(s) >= reinterpret_cast<intptr_t>(buf().data()) &&
           reinterpret_cast<intptr_t>(s) <=
               reinterpret_cast<intptr_t>(buf().data() + buf().size()) &&
           "Slot pointer out of bounds to retrieve slot number");

    return (reinterpret_cast<intptr_t>(s) - reinterpret_cast<intptr_t>(buf().data())) /
           sizeof(Slot);
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

template <std::size_t BS> struct SlotHeader<BS>::iterator
{
  using difference_type = SlotNum;
  using pointer = Slot *;
  using reference = Slot &;

  explicit iterator(SlotHeader *slots) noexcept : m_slots(slots)
  {
    if (slots == nullptr || slots->isEmpty() || slots->isSlotOutOfBounds(0))
    {
      m_isEnd = true;
      return;
    }

    m_current = slots->getSlot(0);
  }
  iterator() : m_slots(nullptr), m_isEnd(true), m_current() {}
  static iterator end() { return iterator(); }

  reference operator*() noexcept { return *m_current; }
  pointer operator->() noexcept { return m_current; }
  iterator &operator++() { return (*this) + 1; }
  iterator operator++(int)
  {
    iterator tmp = *this;
    ++(*this);
    return tmp;
  }
  iterator &operator+(int n)
  {
    if (m_isEnd || m_slots == nullptr)
      return *this;

    m_currentNum += n;
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
  Slot *m_current;
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
template <typename T> struct NodeCell
{
  u32 payloadSize;
  CellPayload payload;

  explicit NodeCell(const T &data)
  {
    payloadSize = sizeof(T);
    // TODO: overflow
    payload.small.fill(static_cast<std::byte>(0));
    std::memcpy(payload.small.data(), &data, payloadSize);
  }

  template <std::size_t BS> NodeCell(const SlotHeader<BS> &sh, const Slot &s)
  {
    // read the structure of the cell from the slot
    const std::byte *pCell = sh.readCell(s.cellOffset);
    payloadSize = *reinterpret_cast<const u32 *>(pCell);
    // TODO overflow. this might be fine, as NodeCell is just a wrapper around what goes into the
    // slots
    payload = *reinterpret_cast<const CellPayload *>(pCell + sizeof(payloadSize));
    // if we want to read the entire payload, this is where we could follow `payload.overflow`
  }

  T getPayload() const noexcept
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
template <typename T> struct InteriorCell
{
  PageId leftChild = 0;
  NodeCell<T> cell;

  explicit InteriorCell(T data) : cell(data) {}

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
  SlotHeader<PAGE_SIZE - sizeof(common) - sizeof(parent) - sizeof(SlotHeader<0>)> slots;

  bool isRoot() const noexcept { return parent == 0; }
  bool isLeaf() const noexcept { return common.type == PageType::Leaf; }

  /* split the node in half, starting from `start`, copying slots to new cell and deleting from
   * original */
  Page<BTreeHeader> &split(Pager &pager, PageId *retPageId = nullptr);
};
static_assert(offsetof(BTreeHeader, common) == 0, "Common header must be at offset 0");
static_assert(offsetof(BTreeHeader, slots) == (sizeof(BTreeHeader) - sizeof(BTreeHeader::slots)),
              "SlotHeader must be the last member");
/* we determine the btree order from the max cell size. this will need to be determined presumably
 * through benchmarking. Since the payload of the cells is not known, here we are allowing a 32 byte
 * payload. MAX_CELL_SIZE is the maximum total cell size (i.e. payload size + the payload itself)
 * within the slotted page's cell. A cell larger than this size is then extended into the overflow
 * page */
static constexpr std::size_t MAX_CELL_SIZE =
    std::max(sizeof(NodeCell<std::any>), sizeof(InteriorCell<std::any>)) + MAX_CELL_PAYLOAD;
static constexpr std::size_t BTREE_ORDER =
    (PAGE_SIZE - sizeof(BTreeHeader)) / (MAX_CELL_SIZE + sizeof(Slot));
static_assert(BTREE_ORDER > 0, "BTree order must be at least 1");

struct InteriorNode
{
  Page<BTreeHeader> page = Page<BTreeHeader>(Interior);

  /* find the leaf node a cell value would be located in.
   * following the leaf linked list is not needed to find the existence of the value */
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
        assert(s.cellSize == sizeof(InteriorCell<V>) &&
               "Interior search cell should be size of Interior");
        const InteriorCell interior =
            *reinterpret_cast<InteriorCell<V> *>(currentPage.header()->slots.getCell(s.cellOffset));
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
        const V &value = interior.cell.getPayload();
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

  template <typename T> void insert(Pager &pager, const T &V)
  {
    Page<BTreeHeader> &leaf = searchGetLeaf(pager, V);
    // BTREE_ORDER is the maximum number of entries in the node, including the end cell
    // leaf nodes do not have an end cell
    if (leaf.header()->slots.entryCount() < BTREE_ORDER)
    {
      leaf.header()->slots.insertCell(LeafCell(V));
      return;
    }

    PageId newNodePageId = 0;
    auto newNode = leaf.header()->split(pager, &newNodePageId);
    assert(newNode.header()->slots.entryCount() < BTREE_ORDER &&
           "Newly split node should have empty slots");
    assert(leaf.header()->slots.entryCount() < BTREE_ORDER &&
           "Newly split node should have empty slots");

    // the new node now contains the low half, so use the lowest value in the new node as the key
    // for it in the parent
    const auto lowestValueCell = reinterpret_cast<const LeafCell<T> *>(
        leaf.header()->slots.getSlotAndCell(static_cast<SlotNum>(0), nullptr));

    if (!leaf.header()->isRoot())
    {
      auto parent = pager.getPage<BTreeHeader>(leaf.header()->parent);
      assert(parent.header()->common.type == PageType::Interior &&
             "Leaf node's parent must be interior node");
      auto keyForNewNode = reinterpret_cast<InteriorCell<T> *>(
          parent.header()->slots.createNextSlotWithCell(sizeof(*lowestValueCell), nullptr));
      keyForNewNode->cell = NodeCell(lowestValueCell->getPayload());
      keyForNewNode->leftChild = newNodePageId;
      newNode.header()->slots.insertCell(LeafCell(V));
    }

    // move the middle item to the parent node
    // here, that is the last item of the original node
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
    header->slots.freeLength -= sizeof(Reserved);

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

  /* find the slot for Q and return the slot and cell */
  template <typename T>
  std::optional<std::pair<Slot *, LeafCell<T> *>> searchGetSlot(const LeafCell<T> &Q)
  {
    // TODO: binary search?
    for (Slot &s : page.header()->slots)
    {
      // TODO: overflow
      LeafCell<T> *c = reinterpret_cast<LeafCell<T> *>(page.header()->slots.getCell(s.cellOffset));
      // TODO: ordering
      if (Q.payload.small == c->payload.small)
      {
        // TODO: overflow
        return std::pair(&s, c);
      }
    }

    return std::nullopt;
  }

private:
  inline Reserved *reserved()
  {
    return reinterpret_cast<Reserved *>(page.buf.data() + page.buf.size() - sizeof(Reserved));
  }
};

template <std::size_t BS>
template <typename P>
Slot *SlotHeader<BS>::insertCell(const InteriorCell<P> &cell, u16 *retSlotNumber)
{
  // TODO: can we use the operator< for InteriorCell instead?
  // since they both have to be the same payload type
  const auto interiorComp = std::function(
      [](const InteriorCell<P> &a, const InteriorCell<P> &b)
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
        return a.cell.getPayload() < b.cell.getPayload();
      });

  return insertCell(cell, interiorComp, retSlotNumber);
}

template <std::size_t BS>
template <typename P>
Slot *SlotHeader<BS>::insertCell(const LeafCell<P> &cell, u16 *retSlotNumber)
{
  // TODO: can we use the operator< for LeafCell instead?
  // since they both have to be the same payload type
  const auto leafComp = std::function([](const LeafCell<P> &a, const LeafCell<P> &b)
                                      { return a.getPayload() < b.getPayload(); });

  return insertCell(cell, leafComp, retSlotNumber);
}

template <typename T> void printTree(Pager &pager, Page<BTreeHeader> &root, u32 depth = 0)
{
  switch (root.header()->common.type)
  {
  case PageType::Interior:
    std::cout << '(';
    for (const auto &slot : root.header()->slots)
    {
      auto cell =
          reinterpret_cast<InteriorCell<T> *>(root.header()->slots.getCell(slot.cellOffset));
      if (cell->isEnd())
      {
        std::cout << "END" << ' ';
      }
      else
      {
        std::cout << cell->cell.getPayload() << ' ';
      }
      Page<BTreeHeader> &child = pager.getPage<BTreeHeader>(cell->leftChild);
      printTree<T>(pager, child, depth + 1);
    }
    std::cout << ')';
    break;
  case PageType::Leaf:
    std::cout << '[';
    for (const auto &slot : root.header()->slots)
    {
      auto cell = reinterpret_cast<LeafCell<T> *>(root.header()->slots.getCell(slot.cellOffset));
      std::cout << cell->getPayload() << ' ';
    }
    std::cout << ']' << ' ';
    break;
  default:
    std::cout << '?' << root.header()->common.type << '?';
  }

  if (depth == 0)
  {
    std::cout << std::endl;
  }
}
