#pragma once

#include "pf_pager.h"
#include "error.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <unordered_map>
#include <algorithm>

class PfManager {
    static std::unordered_map<std::string, int> _path2fd;
    static std::unordered_map<int, std::string> _fd2path;
public:
    static PfPager pager;

    static bool is_file(const std::string &path) {
        struct stat st;
        return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
    }

    static void create_file(const std::string &path) {
        if (is_file(path)) { throw FileExistsError(path); }
        int fd = open(path.c_str(), O_CREAT, S_IRUSR | S_IWUSR);
        if (fd < 0) { throw UnixError(); }
        if (close(fd) != 0) { throw UnixError(); }
    }

    static void destroy_file(const std::string &path) {
        if (!is_file(path)) { throw FileNotFoundError(path); }
        // If file is open, cannot destroy file
        if (_path2fd.count(path)) { throw FileNotClosedError(path); }
        // Remove file from disk
        if (unlink(path.c_str()) != 0) { throw UnixError(); }
    }

    static int open_file(const std::string &path) {
        if (!is_file(path)) { throw FileNotFoundError(path); }
        if (_path2fd.count(path)) {
            // File is already open
            throw FileNotClosedError(path);
        }
        // Open file and return the file descriptor
        int fd = open(path.c_str(), O_RDWR);
        if (fd < 0) { throw UnixError(); }
        // Memorize the opened unix file descriptor
        _path2fd[path] = fd;
        _fd2path[fd] = path;
        return fd;
    }

    static void close_file(int fd) {
        if (!_fd2path.count(fd)) { throw FileNotOpenError(fd); }
        pager.flush_file(fd);
        std::string filename = _fd2path[fd];
        _path2fd.erase(filename);
        _fd2path.erase(fd);
        if (close(fd) != 0) { throw UnixError(); }
    }
};
