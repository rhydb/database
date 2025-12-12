#pragma once

#include "row.hpp"
#include "pages/page_header.hpp"

#include <cstring>
#include <unordered_map>
#include <sstream>

// template the header so that we can retrieve it easily
struct BTreeHeader;

template <typename Header = CommonHeader>
struct Page
{
  std::array<std::byte, PAGE_SIZE> buf = {static_cast<std::byte>(0)};

  Page() : Page(Leaf) {}

  explicit Page(PageType type)
  {
    std::memset(buf.data(), 0, buf.size());

    // TODO: endianness

    // setup the custom header

    Header *h = header();
    if constexpr (std::is_same_v<Header, BTreeHeader>) {
      *h = Header(buf);
    } else {
      *h = Header();
    }

    // setup the common header
    CommonHeader *ch = reinterpret_cast<CommonHeader *>(h);
    ch->type = type;
  }

  // cast the current header to a different header type
  // this is intended to be used when reading the page from disk and relying on the `type` value
  /* return a pointer to the page, casted to a certain page header type */
  template<typename H> // pointer to a page header type
  constexpr Page<H> *as()
  {
    return reinterpret_cast<Page<H>*>(this);
  }

  // when header type is non pointer return a reference to the page instead
  template<typename H>
  Page<H> &as_ref()
  {
    // return *std::launder(reinterpret_cast<Page<H>*>(this));
    return reinterpret_cast<Page<H>&>(*this);
  }

  Header *header() noexcept
  {
    return reinterpret_cast<Header *>(buf.data());
  }
};


// the freelist pages are kept as a linked list with the head stored in the `DatabaseHeader`
struct FreelistPage
{
  struct Header
  {
    CommonHeader common;
    PageId next = 0;
  };

  Page<Header> page = Page<Header>(PageType::Freelist);

  explicit FreelistPage(PageId next)
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

  Page<Header> page = Page<Header>(PageType::First);
};

// overflow pages keep a next pointer followed by as much data as can fit
struct OverflowPage
{
  struct Header
  {
    CommonHeader common;
    // we don't need to keep track of the size here because the total size is with the cell
    // TODO: dont store the next pointer on the last page
    PageId next = 0;
  };

  Page<Header> page = Page<Header>(PageType::Overflow);
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
  
  template<typename H = CommonHeader> // page header type
  Page<H> &getPage(PageId pageNum);
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
};
