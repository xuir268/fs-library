#pragma once

#include "../buffer_pool.hpp"
#include <cstddef>
#include <cstdint>

namespace storage {

constexpr std::size_t BLOCK_SIZE = 4096;

struct Block {
  using block_id_t = std::uint64_t;

  block_id_t id;
  BufferHandle data;

  Block(block_id_t id_, BufferHandle &&buf) : id(id_), data(std::move(buf)) {}

  std::byte *bytes() noexcept { return data.data(); }
  const std::byte *bytes() const noexcept { return data.data(); }
};
} // namespace storage