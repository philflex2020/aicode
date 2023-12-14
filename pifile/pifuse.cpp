#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <map>

//g++ pifuse.cpp -o pifuse -D_FILE_OFFSET_BITS=64 -I/usr/include/fuse -pthread -lfuse
// ./pifuse ~/fuse_mount
// fusermount -u ~/fuse_mount 
static const char *writeFilePath = "/write_file.txt";
std::map<std::string, std::vector<char>> file_contents;

static int my_getattr(const char *path, struct stat *stbuf) {
    int res = 0;
    memset(stbuf, 0, sizeof(struct stat));

    if(strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
    } else if(strcmp(path, writeFilePath) == 0) {
        stbuf->st_mode = S_IFREG | 0666;
        stbuf->st_nlink = 1;
        stbuf->st_size = file_contents[writeFilePath].size();
    } else
        res = -ENOENT;

    return res;
}

static int my_open(const char *path, struct fuse_file_info *fi) {
    return 0;
}

static int my_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    std::vector<char>& content = file_contents[path];
    if (offset >= content.size()) {
        return 0;
    }
    if (offset + size > content.size()) {
        size = content.size() - offset;
    }
    std::copy(content.begin() + offset, content.begin() + offset + size, buf);
    return size;
}

static int my_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    std::vector<char>& content = file_contents[path];
    if (offset + size > content.size()) {
        content.resize(offset + size);
    }
    std::copy(buf, buf + size, content.begin() + offset);
    return size;
}

static struct fuse_operations my_oper = {
    .getattr = my_getattr,
    .open = my_open,
    .read = my_read,
    .write = my_write,
};

int main(int argc, char *argv[]) {
    file_contents[writeFilePath] = std::vector<char>(); // Initialize empty file content
    return fuse_main(argc, argv, &my_oper, NULL);
}

