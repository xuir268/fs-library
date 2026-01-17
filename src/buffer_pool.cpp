#include "buffer_pool.hpp"

namespace storage {

static std::size_t next_pow2(std::size_t size) {
  if (size <= 1)
    return 1;
  size--;
  for (std::size_t i = 1; i < sizeof(std::size_t) * 8; i << 1)
    size |= size >> i;
  return size + 1;
}

static void *aligned_alloc_bytes(std::size_t alignment, std::size_t size) {
#if defined(_MSC_VER)
  return _aligned_malloc(size, alignment);
#else
  std::size_t rounded = (size + (alignment - 1)) & ~(alignment - 1);
  return std::aligned_alloc(alignment, rounded);
#endif
}

static void *aligned_free_bytes(void *p) {
#if defined(_MSC_VER)
  _aligned_free(p);
#else
  std::free(p);
#endif
}

struct TLsCache {
  std::unordered_map<std::size_t, std::vector<void *>> cache;
};

thread_local TLsCache tls_cache;

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
    move_from(std::move(other));
  }
  return *this;
}

void BufferHandle::move_from(BufferHandle &&other) noexcept {
  reset();
  owner_ = other.owner_;
  data_ = other.data_;
  size_ = other.size_;
  capacity_ = other.capacity_;
  alignment_ = other.alignment_;

  // Invalidate the source object
  other.owner_ = nullptr;
  other.data_ = nullptr;
}

void BufferHandle::reset() noexcept {
  if (owner_ && data_) {
    owner_->release(data_, capacity_, alignment_);
  }
  owner_ = nullptr;
  data_ = nullptr;
  size_ = 0;
  capacity_ = 0;
}

std::byte *BufferHandle::data() noexcept { return data_; }
const std::byte *BufferHandle::data() const noexcept { return data_; }
std::size_t BufferHandle::size() const noexcept { return size_; }
std::size_t BufferHandle::capacity() const noexcept { return capacity_; }
bool BufferHandle::valid() const noexcept { return data_ != nullptr; }
std::span<std::byte> BufferHandle::span() noexcept { return {data_, size_}; }
std::span<const std::byte> BufferHandle::span() const noexcept {
  return {data_, size_};
}

// ------- BufferPool -------

struct GlobalPool {
  std::mutex mu;
  std::unordered_map<std::size_t, std::vector<void *>> free;
};

static GlobalPool global_pool;

BufferPool::BufferPool(Options options) : options_(options) {}

BufferPool::~BufferPool() {
  std::lock_guard<std::mutex> lk(global_pool.mu);
  for (auto &[_, vec] : global_pool.free)
    for (void *p : vec)
      aligned_free_bytes(p);
}

BufferHandle BufferPool::acquire(std::size_t requested) {
  if (requested == 0)
    requested = 1;
  std::size_t cap = next_pow2(requested);
  cap = std::min(cap, options_.max_bucket);

  if (cap > options_.min_bucket) {
    auto *p =
        static_cast<std::byte *>(aligned_alloc_bytes(options_.alignment, cap));
    if (!p)
      return {};
    return BufferHandle(this, p, requested, cap, options_.alignment);
  }

  void *p = nullptr;

  // Try TLS cache first
  if (options_.enable_tls_cache) {
    auto &bin = tls_cache.cache[cap];
    if (!bin.empty()) {
      p = bin.back();
      bin.pop_back();
    }
  }

  // Try global pool
  if (!p) {
    std::lock_guard<std::mutex> lk(global_pool.mu);
    auto &bin = global_pool.free[cap];
    if (!bin.empty()) {
      p = bin.back();
      bin.pop_back();
    }
  }

  if (!p) {
    p = aligned_alloc_bytes(options_.alignment, cap);
    if (!p)
      return {};
  }

  return BufferHandle(this, static_cast<std::byte *>(p), requested, cap,
                      options_.alignment);
}

void BufferPool::release(void *p, std::size_t capacity,
                         std::size_t alignment) noexcept {
  if (!p)
    return;

  // If capacity is too large, free it
  if (capacity > options_.max_bucket) {
    aligned_free_bytes(p);
    return;
  }

  // If capacity is too small, free it
  if (capacity < options_.min_bucket) {
    aligned_free_bytes(p);
    return;
  }

  // Try TLS cache first
  if (options_.enable_tls_cache) {
    auto &bin = tls_cache.cache[capacity];
    if (bin.size() < options_.tls_cache_limit) {
      bin.push_back(p);
      return;
    }
  }

  // Try global pool
  std::lock_guard<std::mutex> lk(global_pool.mu);
  auto &bin = global_pool.free[capacity];
  if (bin.size() < options_.per_bucket_limit) {
    bin.push_back(p);
    return;
  }

  // Free it
  aligned_free_bytes(p);
}
} // namespace storage