#include "database/pager.hpp"

Pager::Pager(std::iostream &stream)
: m_stream(stream) {
  // m_stream.open(file, std::ios::in | std::ios::out | std::ios::binary | std::ios::app);
  if (!m_stream)
  {
    throw std::runtime_error("Failed to open database file.");
  }
 
  m_stream.seekg(0, std::ios::end);
  m_fSize =  m_stream.tellg();
  if (!m_stream)
  {
    throw std::runtime_error("Failed to get size of database file.");
  }

  if (m_fSize == 0)
  {
    // new database, write the database header
    firstPage.header()->common.type = PageType::First;

    setPage(0, firstPage.as<CommonHeader>());
    flushPage(0, firstPage);
  }
  else
  {
    firstPage = getPage(0).as<FirstPage::Header>();
  }
}

Page<>& Pager::getPage(PageId pageNum)
{

  if (m_pages.count(pageNum))
  {
    return m_pages[pageNum];
  }

  // read the page and create it in cache
  // TODO: convert the endianness using page type to know the data inside
  // TODO: read the type bytes manually then call the appropiate deserialise on that type
  if (!m_stream.seekg(pageNum * PAGE_SIZE))
  {
    throw PageError(pageNum, "Failed in seeking to read");
  }
  if (!m_stream.read(reinterpret_cast<char*>(m_pages[pageNum].buf.data()), m_pages[pageNum].buf.size()))
  {
    throw PageError(pageNum, "Failed to read");
  }
  
  return m_pages[pageNum];
}

void Pager::setPage(PageId pageNum, const Page<> &page) noexcept
{
  m_pages[pageNum] = page;
}

PageId Pager::nextFree()
{
  // check the firstPage for the free list, otherwise append to file 
  PageId freelist = firstPage.header()->db.freelist;
  if (freelist != 0)
  {
    Page<> &next = getPage(freelist);

    if (next.header()->type == PageType::Freelist)
    {
      // update the head of the linked list
      firstPage.header()->db.freelist = next.as<FreelistPage::Header>().header()->next;
      return freelist;
    }

    // remove the bad free list pointer
    // TODO: we now could have dangling freelist pages
    std::cerr << "Expected freelist page when updating freelist, instead saw " << next.header()->type << std::endl;
    firstPage.header()->db.freelist = 0;
    // continue to create a new page
  }

  // append to file instead
  m_stream.seekg(0, std::ios::end);
  m_fSize = m_stream.tellg();
  PageId nextId = m_fSize / PAGE_SIZE;
  Page<CommonHeader> newPage;
  setPage(nextId, newPage);
  flushPage(nextId, newPage); // write it to disk now
  m_fSize += PAGE_SIZE; // the file size has increased
  return nextId;
}

void Pager::freePage(PageId pageNum)
{
  Page<FreelistPage::Header> &page = getPage(pageNum).as<FreelistPage::Header>();
  page.header()->common.type = Freelist;

  // update the linked list
  page.header()->next = firstPage.header()->db.freelist;;
  firstPage.header()->db.freelist = pageNum;
  // TODO do we need to flush these now?
  // flushPage(pageNum, page);
  // flushPage(0, firstPage);
}
