#include "database/database.hpp"
#include "machine.hpp"

Database::Database(const char *filename)
: pager(filename)
{
}

//
// bool Row::serialise(std::ostream &stream) const noexcept
// {
//   if (!writeNetworku32(stream, id))
//     return false;
//   if (!stream.write(username.data(), username.size()))
//     return false;
//   if (!stream.write(email.data(), email.size()))
//     return false;
//   return true;
// }
//
// bool Row::deserialise(std::istream &stream) noexcept
// {
//   if (!readNetworku32(stream, id))
//     return false;
//   if (!stream.read(username.data(), username.size()))
//     return false;
//   if (!stream.read(email.data(), email.size()))
//     return false;
//   return true;
// }
//
// Table::Table(const char *filename)
// : m_pager(filename)
// {
//   m_nRows = m_pager.fsize() / ROW_SIZE;
// }
//
// Table::~Table()
// {
//   u32 nFullPages = m_nRows / ROWS_PER_PAGE;
//   for (u32 i = 0; i < nFullPages; i++)
//   {
//     if (m_pager.peekCache(i) == nullptr)
//       continue;
//     m_pager.flushPage(i, PAGE_SIZE);
//   }
//
//   u32 nAdditionalRows = m_nRows % ROWS_PER_PAGE;
//   if (nAdditionalRows > 0)
//   {
//     u32 pageNum = nFullPages;
//     if (m_pager.peekCache(pageNum) != nullptr)
//     {
//       m_pager.flushPage(pageNum, nAdditionalRows * ROW_SIZE);
//     }
//   }
// }
//
// char *Table::rowSlot(u32 rowNum)
// {
//   const u32 pageNum = rowNum / ROWS_PER_PAGE;
//   char *page = m_pager.getPage(pageNum);
//   if (page == nullptr)
//   {
//     return nullptr;
//   }
//   u32 rowOffset = rowNum % ROWS_PER_PAGE;
//   u32 byteOffset = rowOffset * ROW_SIZE;
//   return page + byteOffset;
// }
//
// bool Table::insert(Row &row)
// {
//   if (m_nRows >= MAX_ROWS)
//   {
//     return false;
//   }
//
//   char *slot = rowSlot(m_nRows);
//   if (slot == nullptr)
//     return false;
//
//   membuf buf(slot, ROW_SIZE);
//   std::iostream io(&buf);
//
//   if (!row.serialise(io))
//   {
//     std::cerr << "Failed to serialise row" << std::endl;
//     return false;
//   }
//   m_nRows++;
//
//   if (!io.read(slot, ROW_SIZE))
//   {
//     std::cerr << "Failed to write serialised row to slot" << std::endl;
//     return false;
//   }
//
//   return true;
// }
