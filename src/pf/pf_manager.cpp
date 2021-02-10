#include "pf_manager.h"

std::unordered_map<std::string, int> PfManager::_path2fd;
std::unordered_map<int, std::string> PfManager::_fd2path;
PfPager PfManager::pager;
