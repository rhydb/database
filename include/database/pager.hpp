#pragma once

#include <fstream>
#include <memory>
#include <cstring>
#include <unordered_map>
#include <cassert>
#include <sstream>
#include "row.hpp"

using PageId = u32;

const u32 PAGE_SIZE = 512;

struct DatabaseHeader
{
  u16 version = 1;
  PageId freelist = 0; // when 0 no free pages, must be appended to file
};

// we use the slot numbers in the table b tree for row ids
// this is used in indexes to then find the row data
struct Slot
{
  u16 cellOffset; // from the end of the header
  u16 cellSize;   // mark as 0 for free
};

using SlotNum = u16;

// not to be used independently
// used for page types to support slots
// this header should be placed at the end of any other header
// as the slots are expected to be stored immediately after the header
struct SlotHeader
{
  u16 freeStart = 0; // starting from end of header
  u16 freeLength;    // marked as free if 0

  Slot *getSlot(SlotNum slotNumber)
  {
    // go to the end of this header struct, then to the specified slot
    if (slotNumber * sizeof(Slot) >= freeStart)
    {
      throw std::out_of_range("Slot number out of bounds");
    }
    return reinterpret_cast<Slot *>(this + 1) + slotNumber;
  }

  void *getCell(u16 offset)
  {
    char *headerEnd = reinterpret_cast<char *>(this + 1);
    return headerEnd + offset;
  }

  void *getSlotCell(SlotNum slotNumber, Slot *retSlot)
  {
    Slot *s = getSlot(slotNumber);
    if (retSlot != nullptr)
    {
      *retSlot = *s;
    }
    return getCell(s->cellOffset);
  }

  void freeSlot(SlotNum slotNum)
  {
    Slot *s = getSlot(slotNum);
    s->cellSize = 0;
  }

  // TODO: pass a comparison callback
  void *newCell(u16 cellSize, SlotNum *retSlotNumber)
  {
    char *headerEnd = reinterpret_cast<char *>(this + 1);
    Slot *slot = reinterpret_cast<Slot *>(headerEnd + freeStart);
    slot->cellSize = cellSize;

    freeStart += sizeof(Slot);
    slot->cellOffset = freeStart + freeLength - cellSize;

    freeLength -= slot->cellSize + sizeof(Slot); // shrinks from both sides

    if (retSlotNumber != nullptr)
    {
      *retSlotNumber = (reinterpret_cast<intptr_t>(slot) - reinterpret_cast<intptr_t>(headerEnd)) / sizeof(Slot);
    }

    return headerEnd + slot->cellOffset;
  }
};

struct LeafCell
{
  struct Cell
  {
    u32 payloadSize;
    u32 key;
  } cell;
  std::unique_ptr<char[]> data;
};

struct InteriorCell
{
  PageId leftChild;
  u32 key;
};

enum PageType
{
  Root = 0,
  Interior,
  Leaf,
  Freelist,
  First,
  Overflow,
};

// the common header for all pages, must be placed at the beginning
struct CommonHeader
{
  PageType type;
};

// template the header so that we can retrieve it easily
template <typename Header = CommonHeader>
struct Page
{
  std::array<std::byte, PAGE_SIZE> buf;

  Page() : Page(Leaf) {}

  explicit Page(PageType type)
  {
    std::memset(buf.data(), 0, buf.size());

    // TODO: endianness

    // setup the custom header
    Header *h = header();
    *h = Header();

    // setup the common header
    CommonHeader *ch = reinterpret_cast<CommonHeader *>(h);
    ch->type = type;
  }

  // cast the current header to a different header type
  // this is intended to be used when reading the page from disk and relying on the `type` value
  template <typename PageHeader>
  constexpr Page<PageHeader> &as()
  {
    return *reinterpret_cast<Page<PageHeader> *>(this);
  }

  Header *header() noexcept
  {
    return reinterpret_cast<Header *>(buf.data());
  }
};

struct BTreeHeader
{
  CommonHeader common;
  PageId parent = 0;
  SlotHeader slots;
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

// the freelist pages are kept as a linked list with the head stored in the `DatabaseHeader`
struct FreelistPage
{
  struct Header
  {
    CommonHeader common;
    PageId next;
  };

  Page<Header> page;

  explicit FreelistPage(PageId next)
      : page(PageType::Freelist)
  {
    Header *header = page.header();
    header->next = next;
  }

  inline PageId next()
  {
    return page.header()->next;
  }
};

// the first page of the document file located at offset 0
struct FirstPage
{
  struct Header
  {
    CommonHeader common;
    DatabaseHeader db;
  };

  Page<Header> page;

  FirstPage()
      : page(PageType::First)
  {
  }
};

// overflow pages keep a next pointer followed by as much data as can fit
struct OverflowPage
{
  struct Header
  {
    CommonHeader common;
    // we don't need to keep track of the size here because the total size is with the cell
    PageId next = 0;
  };

  Page<Header> page;
  OverflowPage()
      : page(PageType::Overflow)
  {
  }
};

class PageError : std::exception
{
private:
  std::string m_message;
  PageId m_id;

public:
  PageError(PageId id, const char *message)
      : m_id(id)
  {
    std::ostringstream ss;
    ss << "Page " << id << ": " << message << std::endl;
    m_message = ss.str();
  }

  inline PageId id()
  {
    return m_id;
  }

  const char *what() const noexcept override
  {
    return m_message.c_str();
  }
};

// manages the pages for the database
// keeps a cache that is flushed to disk
class Pager
{
public:
  u32 fsize() const noexcept { return m_fSize; }
  Page<> &getPage(PageId pageNum);
  void setPage(PageId pageNum, const Page<> &page) noexcept;

  template <typename H = CommonHeader>
  void flushPage(u32 pageNum, const Page<H> &page)
  {
    if (!m_stream.seekp(pageNum * PAGE_SIZE))
    {
      throw PageError(pageNum, "Failed in seeking to flush");
      return;
    }
    if (!m_stream.write(reinterpret_cast<const char *>(page.buf.data()), page.buf.size()))
    {
      throw PageError(pageNum, "Failed to flush");
      return;
    }
  }

  [[nodiscard]] PageId nextFree();
  void freePage(PageId pageNum);

  explicit Pager(std::iostream &stream);

private:
  std::iostream &m_stream;
  // our cache for the pages
  std::unordered_map<PageId, Page<>> m_pages;
  u32 m_fSize;
  Page<FirstPage::Header> &firstPage = m_pages[0].as<FirstPage::Header>();
};
