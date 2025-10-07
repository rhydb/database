#pragma once

#include <stdint.h>

#include "machine.hpp"
#include "pager.hpp"

class Database
{
public:
  explicit Database(std::iostream &stream);
  Pager pager;
};

// class Table
// {
// public:
//   Table(const char* filename);
//
//   bool insert(Row &row);
//   char* rowSlot(u32 rowNum);
//   u32 nRows() const noexcept { return m_nRows; }
//
//   ~Table();
//
//   Table(const Table &) = delete;
//   Table& operator=(const Table&) = delete;
//
//   Table(Table&&) noexcept = default;
//   Table& operator=(Table&&) noexcept = default;
// private:
//   u32 m_nRows = 0;
//   Pager m_pager;
// };
