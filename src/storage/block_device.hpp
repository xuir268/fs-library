#pragma once

#include "block.hpp"
#include <cstdint>
#include <memory>

namespace storage {

class BlockDevice {
public:
  virtual ~BlockDevice() = default;

  virtual void read_block(Block::block_id_t id, Block &out) = 0;
  virtual void write_block(const Block &block) = 0;

  virtual std::uint64_t num_blocks() const noexcept = 0;
  virtual std::size_t block_size() const noexcept = 0;
};
} // namespace storage