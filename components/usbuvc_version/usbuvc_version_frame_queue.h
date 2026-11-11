// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>

namespace esphome::usbuvc_version {

// Latest-frame mailbox over two externally-owned PSRAM buffers. Caller must
// lock around push/take. Only the consumer calls take; its returned buffer
// stays untouched by push until the consumer next calls take.
class JpegFrameQueue {
 public:
  void setup(uint8_t *pending, uint8_t *worker, size_t capacity) {
    pending_ = pending;
    worker_ = worker;
    capacity_ = capacity;
    length_ = 0;
  }
  bool push(const uint8_t *data, size_t length) {
    if (data == nullptr || pending_ == nullptr || length == 0 || length > capacity_) return false;
    std::memcpy(pending_, data, length);
    length_ = length;
    return true;
  }
  bool take(const uint8_t *&data, size_t &length) {
    if (length_ == 0) return false;
    std::swap(pending_, worker_);
    data = worker_;
    length = length_;
    length_ = 0;
    return true;
  }
 private:
  uint8_t *pending_{nullptr};
  uint8_t *worker_{nullptr};
  size_t capacity_{0};
  size_t length_{0};
};

}  // namespace esphome::usbuvc_version
