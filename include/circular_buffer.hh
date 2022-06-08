/**
 * This file is part of rtutils distribution
 *
 * Copyright (C) 2022 Ayan Shafqat <ayan.x.shafqat@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 **/

#ifndef RTUTIL_LOCKFREE_CIRCULARBUFFER_HH_
#define RTUTIL_LOCKFREE_CIRCULARBUFFER_HH_

#include <atomic>
#include <bit>
#include <cassert>
#include <limits>
#include <vector>

#ifdef _MSC_VER
#include <intrin.h>
#endif

constexpr bool is_pow2(std::size_t z) {
  if (z > 0) {
    return ((z & (z - 1U)) == 0);
  } else {
    return false;
  }
}

constexpr std::size_t next_pow2(std::size_t z) {
  if (z <= 2U) {
    return 2U;
  } else if (is_pow2(z)) {
    return z;
  } else {
#if __cplusplus >= 202002L
    constexpr auto n_bits = std::numeric_limits<std::size_t>::digits;
    std::size_t lg2 = (n_bits - 1U) - std::countl_zero(z);
#elif defined (__GNUC__)
    std::size_t lg2 = 31U - __builtin_clz(z);
#elif defined (_MSC_VER)
     std::size_t lg2 = std::size_t(31U) - __lzcnt64(z);
#endif
    return std::size_t(1U) << (lg2 + 1);
  }
}

/**
 * @brief An implementation of a naiive lock-free single producer
 *  and single consumer circular buffer
 * @tparam DataType Data type of the circular buffer element
 */
template <typename DataType>
class CircularBuffer {
 public:
  /**
   * @brief Construct a new Circular buffer object
   */
  CircularBuffer() = default;

  /**
   * @brief Construct a new circular buffer object
   * @param capacity Capacity of circular buffer
   */
  CircularBuffer(std::size_t capacity)
      : buffer_(next_pow2(capacity)), read_head_{0}, write_head_{0} {}

  /**
   * @brief Get circular buffer capacity
   * @return std::size_t circular buffer capacity
   */
  std::size_t capacity() const { return buffer_.size(); }

  /**
   * @brief Resize circular buffer
   * @note This method is not thread safe
   * @param new_size New circular buffer size
   */
  void resize(std::size_t new_size) { buffer_.resize(next_pow2(new_size)); }

  /**
   * @brief Get the read space available
   * @return std::size_t Number of elements available
   */
  std::size_t get_read_available() const {
    return compute_read_available(read_head_.load(), write_head_.load(),
                                  capacity());
  }

  /**
   * @brief Get the write space available
   * @return std::size_t Number of elements available
   */
  std::size_t get_write_available() const {
    return compute_write_available(read_head_.load(), write_head_.load(),
                                   capacity());
  }

  /**
   * @brief Read from the circular buffer
   * @param[out] dst Pointer to the destination buffer
   * @param size Elements requested
   * @return std::size_t Number of elements read
   */
  std::size_t dequeue(DataType *dst, std::size_t size) noexcept {
    auto read_start = read_head_.load();
    auto write_start = write_head_.load();
    auto ring_size = capacity();
    auto ring_mask = ring_size - 1;

    auto read_available =
        compute_read_available(read_start, write_start, ring_size);
    auto read_size = std::min(size, read_available);
    auto read_end = read_start + read_size;

    // Compute the segment sizes
    auto cnt_hi = ring_size - read_start;
    auto cnt_lo = read_end & ring_mask;
    auto buf_begin = buffer_.begin();
    auto buf_end = buffer_.end();

    if (read_end > ring_size) {
      std::copy(buf_begin + read_start, buf_end, dst);  // Copy high segment
      std::copy(buf_begin, buf_begin + cnt_lo,
                dst + cnt_hi);  // Copy low segment
    } else {
      auto start = buf_begin + read_start;
      auto stop = start + read_size;
      std::copy(start, stop, dst);
    }

    // Store the updated read offset
    auto read_head = cnt_lo;
    read_head_.store(read_head);
    return read_size;
  }

  /**
   * @brief Write to the circular buffer
   * @param[in] src Pointer to the source buffer
   * @param size Elements need to be written
   * @return std::size_t Number of elements written
   */
  std::size_t enqueue(DataType const *src, std::size_t size) noexcept {
    auto read_start = read_head_.load();
    auto write_start = write_head_.load();
    auto ring_size = capacity();
    auto ring_mask = ring_size - 1;

    auto write_available =
        compute_write_available(read_start, write_start, ring_size);
    auto write_size = std::min(size, write_available);
    auto write_end = write_start + write_size;

    // Compute the segment sizes
    auto cnt_hi = ring_size - write_start;
    auto cnt_lo = write_end & ring_mask;
    auto buf_begin = buffer_.begin();

    if (write_end > ring_size) {
      std::copy(src, src + cnt_hi,
                buf_begin + write_start);  // Copy high segment
      std::copy(src + cnt_hi, src + write_size,
                buf_begin);  // Copy low segment
    } else {
      std::copy(src, src + write_size, buf_begin + write_start);
    }

    // Store the updated write offset
    auto write_head = cnt_lo;
    write_head_.store(write_head);
    return write_size;
  }

 private:
  /**
   * @brief Helper function to compute amount of read data availbale
   * @param read_head Current read head position
   * @param write_head Current write head position
   * @param capacity Ring buffer capacity
   * @return std::size_t Amount of elements available for reading
   */
  static std::size_t compute_read_available(std::size_t read_head,
                                            std::size_t write_head,
                                            std::size_t capacity) {
    assert(is_pow2(capacity) == true);

    // Singe ring buffer capacity is always a power of two, the modulo
    // operation can be replaced by a bit-mask
    auto mask = capacity - 1U;
    std::size_t read_available = 0U;

    if (write_head > read_head) {
      read_available = write_head - read_head;
    } else {
      read_available = (capacity - write_head + read_head) & mask;
    }

    assert(read_available < capacity);
    return read_available;
  }

  /**
   * @brief Helper function to compute amount of write data availbale
   * @param read_head Current read head position
   * @param write_head Current write head position
   * @param capacity Ring buffer capacity
   * @return std::size_t Amount of elements available for writing
   */
  static std::size_t compute_write_available(std::size_t read_head,
                                             std::size_t write_head,
                                             std::size_t capacity) {
    return capacity - compute_read_available(read_head, write_head, capacity) -
           1U;
  }

 private:
  std::vector<DataType> buffer_{};
  std::atomic<std::size_t> read_head_{0};
  std::atomic<std::size_t> write_head_{0};
};

#endif /* RTUTIL_LOCKFREE_CIRCULARBUFFER_HH_ */
