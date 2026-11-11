// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>

namespace esphome::usbuvc_version {

struct DecodedFrame {
  uint16_t width{0};
  uint16_t height{0};
  size_t length{0};
};

enum class JpegDecodeResult { OK, INVALID_JPEG, SIZE_LIMIT, DECODE_FAILED };

// Decode into an owned, bounded RGB565 big-endian buffer. Dimensions come
// from the JPEG header; configured dimensions are limits, never assumptions.
JpegDecodeResult decode_jpeg(const uint8_t *jpeg, size_t length, uint8_t *rgb, size_t capacity,
                             uint16_t max_width, uint16_t max_height, uint8_t *work, size_t work_size, DecodedFrame &frame);

}  // namespace esphome::usbuvc_version
