#pragma once

#include <gtest/gtest.h>
#include <filesystem>

class TempFileFixture : public ::testing::Test
{
protected:
  void SetUp() override
  {
    m_name = makeUniqueName();
    path = std::filesystem::temp_directory_path() / m_name;
  }

  void TearDown() override {
    if (HasFailure())
    {
      std::cerr << "Failed database file at: " << path << std::endl;
    }
    else
    {
      if (std::filesystem::exists(path))
      {
        std::filesystem::remove(path);
      }
    }
  }

  std::filesystem::path path;
    
private:
    std::string makeUniqueName()
    {
      auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
#if defined(_WIN32)
      unsigned long pid = GetCurrentProcessId();
#else
      unsigned long pid = static_cast<unsigned long>(::getpid());
#endif
      char buf[128];
      std::snprintf(buf, sizeof(buf), "test_tmp_%lx_%lx", now, pid);
      return std::string(buf);
    }

    std::string m_name;
};
