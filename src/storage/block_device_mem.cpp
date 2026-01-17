#pragma once
#include "block_device.hpp"

#include <cstring>
#include <mutex>
#include <unordered_map>

namespace storage {

class InMemoryBlockDevice : public storage::BlockDevice {
public:
  ~InMemoryBlockDevice() override = default;

  explicit InMemoryBlockDevice(storage::BufferPool &pool) : pool_(pool) {}

  std::size_t block_size() const noexcept override { return BLOCK_SIZE; }

  std::uint64_t num_blocks() const noexcept override {
    std::lock_guard<std::mutex> lk(mu_);
    return blocks_.size();
  }

  void read_block(storage::Block::block_id_t id, storage::Block &out) override {
    std::lock_guard<std::mutex> lk(mu_);
    auto it = blocks_.find(id);
    if (it == blocks_.end()) {
      std::memset(out.bytes(), 0, BLOCK_SIZE);
      return;
    }
    std::memcpy(out.bytes(), it->second.data(), BLOCK_SIZE);
  }
  void write_block(const Block &block) override {
    std::lock_guard<std::mutex> lk(mu_);
    auto &buf = blocks_[block.id];
    if (!buf.valid()) {
      buf = pool_.acquire(BLOCK_SIZE);
    }
    std::memcpy(buf.data(), block.bytes(), BLOCK_SIZE);
  }

private:
  BufferPool &pool_;
  mutable std::mutex mu_;
  std::unordered_map<Block::block_id_t, BufferHandle> blocks_;
};

} // namespace storage
