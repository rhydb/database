#pragma once

#include "page_header.hpp"
#include "../pager.hpp"

#include <any>
#include <algorithm>
#include <cassert>
#include <span>
#include <functional>
#include <optional>
#include <type_traits>

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
private:
  template <bool IsConst> struct iteratorbase;

public:
  struct iterator : public iteratorbase<false>
  {
    explicit iterator(SlotHeader<bufsize> *p) : iteratorbase<false>(p) {}
    iterator() : iteratorbase<false>() {}
  };
  struct const_iterator : public iteratorbase<true>
  {
    explicit const_iterator(const SlotHeader<bufsize> *p) : iteratorbase<true>(p) {}
    const_iterator() : iteratorbase<true>() {}
  };

  iterator begin() { return iterator(this); }
  iterator end() { return iterator(); }
  const_iterator begin() const { return const_iterator(this); }
  const_iterator end() const { return const_iterator(); }

  u16 freeStart = 0;        // offset from end of header
  u16 freeLength = bufsize; // marked as free if both 0

  bool isEmpty() const noexcept { return freeStart == 0; }
  bool isFree() const noexcept { return freeStart == 0 && freeLength == 0; }
  u16 entryCount() const noexcept { return freeStart / sizeof(Slot); }

  bool isSlotOutOfBounds(SlotNum slotNumber) const noexcept { return slotNumber * sizeof(Slot) > freeStart; }

  Slot *getSlot(SlotNum slotNumber)
  {
    if (isSlotOutOfBounds(slotNumber))
    {
      throw std::out_of_range("Slot number out of bounds");
    }
    return reinterpret_cast<Slot *>(buf().data() + slotNumber * sizeof(Slot));
  }
  const Slot *getSlot(SlotNum slotNumber) const
  {
    return const_cast<const Slot *>(const_cast<SlotHeader<bufsize> *>(this)->getSlot(slotNumber));
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
  inline const std::byte *readCell(u16 offset) const { return buf().data() + offset; }

  // delete the slot from the slot array.
  void deleteSlot(SlotNum slotNum)
  {
    Slot *s = getSlot(slotNum);
    deleteSlot(&s);
  }

  // copy the cell into the slot buffer and create its slot in the sorted position
  // returns the slot that points to the cell in the slot buffer
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

  // template <typename P> Slot *insertCell(const InteriorCell<P> &cell, u16 *retSlotNumber =
  // nullptr); template <typename P> Slot *insertCell(const LeafCell<P> &cell, u16 *retSlotNumber =
  // nullptr);

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

template <std::size_t BS> template <bool IsConst> struct SlotHeader<BS>::iteratorbase
{
  using container_type = std::conditional_t<IsConst, const SlotHeader<BS>, SlotHeader<BS>>;
  using value_type = std::conditional_t<IsConst, const Slot, Slot>;
  using container_pointer = container_type *;
  using difference_type = SlotNum;
  using pointer = value_type *;
  using reference = value_type &;

  explicit iteratorbase(container_pointer slots) noexcept : m_slots(slots)
  {
    if (slots == nullptr || slots->isEmpty() || slots->isSlotOutOfBounds(0))
    {
      m_isEnd = true;
      return;
    }

    m_current = slots->getSlot(0);
  }
  iteratorbase() : m_slots(nullptr), m_isEnd(true), m_current() {}
  static iteratorbase end() { return iteratorbase(); }

  reference operator*() noexcept { return *m_current; }
  pointer operator->() noexcept { return m_current; }
  iteratorbase &operator++() { return (*this) + 1; }
  iteratorbase operator++(int)
  {
    iteratorbase tmp = *this;
    ++(*this);
    return tmp;
  }
  iteratorbase &operator+(int n)
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
  friend bool operator==(const iteratorbase &a, const iteratorbase &b)
  {
    if (a.m_isEnd && b.m_isEnd)
      return true;
    return a.m_isEnd == b.m_isEnd && a.m_slots == b.m_slots && a.m_current == b.m_current;
  }
  friend bool operator!=(const iteratorbase &a, const iteratorbase &b) { return !(a == b); }

private:
  container_pointer m_slots = nullptr;
  bool m_isEnd = false;
  pointer m_current;
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
    // TODO: overflow. this might be fine, as NodeCell is just a wrapper around what goes into the
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

  friend bool operator<(const NodeCell<T> &a, const NodeCell<T> &b)
  {
    // TODO: optimise payload comparisons
    return a.getPayload() < b.getPayload();
  }

  friend bool operator<(const NodeCell<T> &a, const InteriorCell<T> &b)
  {
    if (b.isEnd())
    {
      return true;
    }
    // TODO: optimise payload comparisons
    return a.getPayload() < b.cell.getPayload();
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

  explicit InteriorCell(const T &data) : cell(data) {}

  bool isEnd() const noexcept { return cell.payloadSize == 0; }

  static InteriorCell End() noexcept
  {
    InteriorCell c(0);
    c.leftChild = 0;
    c.cell.payloadSize = 0;
    return c;
  }

  friend bool operator<(const InteriorCell<T> &a, const T &b)
  {
    if (a.isEnd())
    {
      return false;
    }
    // TODO: optimise payload comparisons
    return a.cell.getPayload() < b;
  }
  friend bool operator<(const InteriorCell<T> &a, const InteriorCell<T> &b)
  {
    if (a.isEnd())
    {
      return false;
    }
    if (b.isEnd())
    {
      return true;
    }
    // TODO: optimise payload comparisons
    return a.cell.getPayload() < b.cell.getPayload();
  }

  friend bool operator<(const InteriorCell<T> &a, const NodeCell<T> &b)
  {
    if (a.isEnd())
    {
      return false;
    }
    // TODO: optimise payload comparisons
    return a.cell.getPayload() < b.getPayload();
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

  template <typename T> T getLowestPayload()
  {
    assert(slots.entryCount() > 0 && "Getting lowest payload requires at least 1 entry");
    const auto p = slots.getSlotAndCell(static_cast<SlotNum>(0), nullptr);
    switch (common.type)
    {
    case PageType::Interior:
      return reinterpret_cast<const InteriorCell<T> *>(p)->cell.getPayload();
      break;
    case PageType::Leaf:
      return reinterpret_cast<const LeafCell<T> *>(p)->getPayload();
      break;
    default:
      throw "Page type must Interior or Leaf";
    }
  }
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
  Page<BTreeHeader> &page;

  /* find the leaf node a cell value would be located in.
   * following the leaf linked list is not needed to find the existence of the value */
  template <typename V> Page<BTreeHeader> &searchGetLeaf(Pager &pager, const V &Q)
  {
    // find the node to follow down
    // TODO: maybe a binary search here to find the node

    Page<BTreeHeader> *currentPage = &this->page;

    while (!currentPage->header()->isLeaf())
    {
      for (const Slot &s : currentPage->header()->slots)
      {
        // get the interior cell for this slot
        assert(s.cellSize == sizeof(InteriorCell<V>) &&
               "Interior search cell should be size of Interior");
        const InteriorCell interior = *reinterpret_cast<InteriorCell<V> *>(
            currentPage->header()->slots.getCell(s.cellOffset));
        assert(interior.leftChild != 0 &&
               "Non leaf node cannot have leaf cell. Tree must be unbalanced");
        if (interior.isEnd())
        {
          // end cell of this node, follow it down
          currentPage = &pager.getPage<BTreeHeader>(interior.leftChild);
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
          currentPage = &pager.getPage<BTreeHeader>(interior.leftChild);
          break;
        }
      }
    }

    return *currentPage;
  }
};

struct LeafNode
{
  Page<BTreeHeader> &page;

  struct Reserved
  {
    PageId sibling;
  };

  explicit LeafNode(Page<BTreeHeader> &existingPage) : page(existingPage) {};

  // LeafNode() : page(Page<BTreeHeader>())
  // {
  //   BTreeHeader *header = page.header();
  //   header->slots.freeLength -= sizeof(Reserved);

  //   setSibling(0);
  // }

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

template <typename T> InteriorCell<T> createKeyForNewNode(PageId pageId) {}

/* after splitting a node, insert a value based on the median of the two. It would be assumed that
 * `median` is the first value in `higher` */
template <typename T, typename K>
void insertByMedianKey(const T &value, const K &medianKey, Page<BTreeHeader> &lower,
                       Page<BTreeHeader> &higher)
{
  if (value < medianKey)
  {
    lower.header()->slots.insertCell(value);
  }
  else
  {
    higher.header()->slots.insertCell(value);
  }
}

template <typename K>
void interiorInsert(Pager &pager, PageId nodeId, Page<BTreeHeader> &node,
                    const InteriorCell<K> &cell)
{
  assert(node.header()->common.type == PageType::Interior &&
         "Interior insert can only be used on interior nodes");

  if (node.header()->slots.entryCount() < BTREE_ORDER)
  {
    node.header()->slots.insertCell(cell);
    return;
  }

  // interior nodes move their middle value up when splitting
  splitAndInsert<K>(pager, nodeId, node, cell, true);
}

template <typename K, typename V>
std::pair<Page<BTreeHeader> &, InteriorCell<K>>
splitAndInsert(Pager &pager, PageId nodeToSplitId, Page<BTreeHeader> &nodeToSplit,
               const V &valueToInsert, bool keyShouldReplaceValue)
{
  PageId newNodePageId = 0;
  auto &newNode = nodeToSplit.header()->split(pager, &newNodePageId);
  assert(newNode.header()->slots.entryCount() < BTREE_ORDER &&
         "New node from splits hould have empty slots");
  assert(nodeToSplit.header()->slots.entryCount() < BTREE_ORDER &&
         "Original node after split should have empty slots");
  assert(nodeToSplit.header()->common.type == newNode.header()->common.type &&
         "Original and new node must have same type");

  // create a key for the new node using the median key
  // HACK: we assume the key is always the first attribute in the payload

  const K medianKey = nodeToSplit.header()->getLowestPayload<K>();
  auto keyForNewNode = InteriorCell(medianKey);
  keyForNewNode.leftChild = newNodePageId;

  if (keyShouldReplaceValue)
  {
    // insert an end cell in the new node to point to where the moved cell pointed to
    InteriorCell<K> endCell = InteriorCell<K>::End();
    {
      const auto medianCell = reinterpret_cast<InteriorCell<K> *>(
          nodeToSplit.header()->slots.getSlotAndCell(0, nullptr));
      endCell.leftChild = medianCell->leftChild;
    }
    nodeToSplit.header()->slots.deleteSlot(static_cast<SlotNum>(0));
    newNode.header()->slots.insertCell(endCell);
  }

  insertByMedianKey(valueToInsert, medianKey, newNode, nodeToSplit);

  Page<BTreeHeader> *parent = nullptr;
  if (!nodeToSplit.header()->isRoot())
  {
    parent = &pager.getPage<BTreeHeader>(nodeToSplit.header()->parent);
  }
  else
  {
    // create a parent
    PageId parentId{};
    parent = &pager.fromNextFree<BTreeHeader>(PageType::Interior, &parentId);
    nodeToSplit.header()->parent = newNode.header()->parent = parentId;
    // link the end node to the original node
    InteriorCell<K> endCell = InteriorCell<K>::End();
    endCell.leftChild = nodeToSplitId;
    parent->header()->slots.insertCell(endCell);
    // we don't need to touch the types of `node` or `newNode`. If the root
    // was a leaf, it still is. If it was internal, it still is.
  }

  assert(!nodeToSplit.header()->isRoot() && "Node cannot be root after splitting");
  interiorInsert<K>(pager, nodeToSplit.header()->parent, *parent, keyForNewNode);
  return std::make_pair(std::ref(newNode), keyForNewNode);
}

template <typename K, typename V>
void leafInsert(Pager &pager, PageId nodeId, Page<BTreeHeader> &node, const V &value)
{
  if (node.header()->slots.entryCount() < BTREE_ORDER)
  {
    node.header()->slots.insertCell(value);
    return;
  }

  // leaf nodes must maintain the linked list between them
  auto [newNode, keyForNewNode] = splitAndInsert<K>(pager, nodeId, node, value, false);
  LeafNode newLeaf{newNode};
  // TODO: set sibling double linked list
  // the left sibling of node still points to node, it should point to the new node
  newLeaf.setSibling(nodeId);
}
