#pragma once

#include "error.h"
#include "pf/pf_pager.h"
#include <algorithm>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <unordered_map>

class PfManager {
  public:
    static PfPager pager;

    static bool is_file(const std::string &path);

    static void create_file(const std::string &path);

    static void destroy_file(const std::string &path);

    static int open_file(const std::string &path);

    static void close_file(int fd);

  private:
    static std::unordered_map<std::string, int> _path2fd;
    static std::unordered_map<int, std::string> _fd2path;
};
