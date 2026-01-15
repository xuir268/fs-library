#include "buffer_pool.hpp"
#include <iostream>
#include <utility>

namespace storage {

// --- BufferHandle Implementation ---

BufferHandle::BufferHandle(BufferPool *owner, std::byte *data, std::size_t size,
                           std::size_t capacity, std::size_t alignment) noexcept
    : owner_(owner), data_(data), size_(size), capacity_(capacity),
      alignment_(alignment) {}

BufferHandle::~BufferHandle() { reset(); }

BufferHandle::BufferHandle(BufferHandle &&other) noexcept {
  move_from(std::move(other));
}

BufferHandle &BufferHandle::operator=(BufferHandle &&other) noexcept {
  if (this != &other) {
    reset();
    move_from(std::move(other));
  }
  return *this;
}

std::byte *BufferHandle::data() noexcept { return data_; }

const std::byte *BufferHandle::data() const noexcept { return data_; }

std::size_t BufferHandle::size() const noexcept { return size_; }

std::size_t BufferHandle::capacity() const noexcept { return capacity_; }

std::span<std::byte> BufferHandle::span() noexcept { return {data_, size_}; }

std::span<const std::byte> BufferHandle::span() const noexcept {
  return {data_, size_};
}

bool BufferHandle::valid() const noexcept { return data_ != nullptr; }

void BufferHandle::reset() noexcept {
  // If we had a real pool logic here, we would notify owner_ to release data_
  owner_ = nullptr;
  data_ = nullptr;
  size_ = 0;
  capacity_ = 0;
  alignment_ = 0;
}

void BufferHandle::move_from(BufferHandle &&other) noexcept {
  owner_ = other.owner_;
  data_ = other.data_;
  size_ = other.size_;
  capacity_ = other.capacity_;
  alignment_ = other.alignment_;

  other.owner_ = nullptr;
  other.data_ = nullptr;
  other.size_ = 0;
  other.capacity_ = 0;
  other.alignment_ = 0;
}

// --- BufferPool Implementation ---

BufferPool::BufferPool() : BufferPool(Options{}) {}

BufferPool::BufferPool(Options options) : options_(options) {
  std::cout << "BufferPool: Initialized with min_bucket=" << options_.min_bucket
            << "\n";
}

BufferPool::~BufferPool() {
  for (auto *ptr : free_list_) {
    delete[] static_cast<std::byte *>(ptr);
  }
  std::cout << "BufferPool: Destroyed and memory freed\n";
}

BufferHandle BufferPool::acquire(std::size_t requested) {
  // Simplified logic: allocate on demand if free_list is empty
  // In a real pool, you'd check free_list_ by bucket size
  std::byte *ptr = new std::byte[requested];
  std::cout << "BufferPool: Allocated " << requested << " bytes\n";
  return BufferHandle(this, ptr, requested, requested, options_.alignment);
}

} // namespace storage
