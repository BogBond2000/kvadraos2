#pragma once

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cstring>
#include <stdexcept>
#include <string>

class SharedMemory {
public:
    SharedMemory(const char* name, size_t size, bool create)
        : name_(name), size_(size), fd_(-1), data_(nullptr) {
        int flags = O_RDWR;
        if (create) {
            flags |= O_CREAT | O_EXCL;
            fd_ = shm_open(name, flags, 0600);
            if (fd_ == -1) {
                throw std::runtime_error("shm_open create failed");
            }
            if (ftruncate(fd_, size_) == -1) {
                close(fd_);
                throw std::runtime_error("ftruncate failed");
            }
        } else {
            fd_ = shm_open(name, flags, 0);
            if (fd_ == -1) {
                throw std::runtime_error("shm_open open failed");
            }
        }

        data_ = mmap(nullptr, size_, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
        if (data_ == MAP_FAILED) {
            close(fd_);
            throw std::runtime_error("mmap failed");
        }
    }

    ~SharedMemory() {
        if (data_ != nullptr && data_ != MAP_FAILED) {
            munmap(data_, size_);
        }
        if (fd_ != -1) {
            close(fd_);
        }
    }

    void* get() const { return data_; }

    void unlink() {
        shm_unlink(name_.c_str());
    }

private:
    std::string name_;
    size_t size_;
    int fd_;
    void* data_;
};