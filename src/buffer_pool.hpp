#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace storage {

class BufferPool; // Forward declaration

class BufferHandle {
public:
  BufferHandle() = default;

  // Move operations
  BufferHandle(BufferHandle &&) noexcept;
  BufferHandle &operator=(BufferHandle &&) noexcept;

  // Disable copy
  BufferHandle(const BufferHandle &) = delete;
  BufferHandle &operator=(const BufferHandle &) = delete;

  ~BufferHandle();

  // Accessors
  [[nodiscard]] std::byte *data() noexcept;
  [[nodiscard]] const std::byte *data() const noexcept;
  [[nodiscard]] std::size_t size() const noexcept;
  [[nodiscard]] std::size_t capacity() const noexcept;
  [[nodiscard]] std::span<std::byte> span() noexcept;
  [[nodiscard]] std::span<const std::byte> span() const noexcept;
  [[nodiscard]] bool valid() const noexcept;
  explicit operator bool() const noexcept { return valid(); }

private:
  friend class BufferPool;
  BufferHandle(BufferPool *owner, std::byte *data, std::size_t size,
               std::size_t capacity, std::size_t alignment) noexcept;

  void reset() noexcept;
  void move_from(BufferHandle &&) noexcept;

  BufferPool *owner_ = nullptr;
  std::byte *data_ = nullptr;
  std::size_t size_ = 0;
  std::size_t capacity_ = 0;
  std::size_t alignment_ = 0;
};

class BufferPool {
public:
  struct Options {
    std::size_t min_bucket = 256;        // smallest pooled size
    std::size_t max_bucket = 1 << 20;    // 1 MiB
    std::size_t alignment = 64;          // cache-line aligned
    std::size_t per_bucket_limit = 1024; // global cap
    bool enable_tls_cache = true;
    std::size_t tls_cache_limit = 64; // per thread per bucket
  };

  BufferPool();
  explicit BufferPool(Options options);
  ~BufferPool();

  BufferPool(const BufferPool &) = delete;
  BufferPool &operator=(const BufferPool &) = delete;

  // Acquire buffer with capacity >= requested
  BufferHandle acquire(std::size_t requested);

private:
  friend class BufferHandle;

  void release(void *p, std::size_t capacity, std::size_t alignment) noexcept;

  Options options_;
  std::vector<void *> free_list_;
};

} // namespace storage