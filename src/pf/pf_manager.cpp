#include "pf/pf_manager.h"

std::unordered_map<std::string, int> PfManager::_path2fd;
std::unordered_map<int, std::string> PfManager::_fd2path;
PfPager PfManager::pager;

bool PfManager::is_file(const std::string &path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

void PfManager::create_file(const std::string &path) {
    if (is_file(path)) {
        throw FileExistsError(path);
    }
    int fd = open(path.c_str(), O_CREAT, S_IRUSR | S_IWUSR);
    if (fd < 0) {
        throw UnixError();
    }
    if (close(fd) != 0) {
        throw UnixError();
    }
}

void PfManager::destroy_file(const std::string &path) {
    if (!is_file(path)) {
        throw FileNotFoundError(path);
    }
    // If file is open, cannot destroy file
    if (_path2fd.count(path)) {
        throw FileNotClosedError(path);
    }
    // Remove file from disk
    if (unlink(path.c_str()) != 0) {
        throw UnixError();
    }
}

int PfManager::open_file(const std::string &path) {
    if (!is_file(path)) {
        throw FileNotFoundError(path);
    }
    if (_path2fd.count(path)) {
        // File is already open
        throw FileNotClosedError(path);
    }
    // Open file and return the file descriptor
    int fd = open(path.c_str(), O_RDWR);
    if (fd < 0) {
        throw UnixError();
    }
    // Memorize the opened unix file descriptor
    _path2fd[path] = fd;
    _fd2path[fd] = path;
    return fd;
}

void PfManager::close_file(int fd) {
    if (!_fd2path.count(fd)) {
        throw FileNotOpenError(fd);
    }
    pager.flush_file(fd);
    std::string filename = _fd2path[fd];
    _path2fd.erase(filename);
    _fd2path.erase(fd);
    if (close(fd) != 0) {
        throw UnixError();
    }
}
