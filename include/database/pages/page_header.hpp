#pragma once

#include "machine.hpp"

using PageId = u32;
const u32 PAGE_SIZE = 512;

// the header for the database file, stored in the first page
struct DatabaseHeader
{
  u16 version = 1;
  PageId freelist = 0; // when 0 no free pages, must be appended to file
};

enum PageType
{
  Leaf,
  Interior,
  Freelist,
  First,
  Overflow,
};

// the common header for all pages, must be placed at the beginning
struct CommonHeader
{
  PageType type;
};
