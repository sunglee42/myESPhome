// SPDX-License-Identifier: GPL-3.0-or-later
#include "usbuvc_version_jpeg.h"
#include "jpeg_decoder.h"


namespace esphome::usbuvc_version {

JpegDecodeResult decode_jpeg(const uint8_t *jpeg, size_t length, uint8_t *rgb, size_t capacity,
                             uint16_t max_width, uint16_t max_height, uint8_t *work, size_t work_size, DecodedFrame &frame) {
  frame = {};
  if (jpeg == nullptr || rgb == nullptr || work == nullptr || work_size < 8192 || length == 0 || length > UINT32_MAX || capacity > UINT32_MAX)
    return JpegDecodeResult::INVALID_JPEG;

  esp_jpeg_image_cfg_t cfg = {};
  cfg.indata = const_cast<uint8_t *>(jpeg);
  cfg.indata_size = length;
  cfg.out_format = JPEG_IMAGE_FORMAT_RGB565;
  cfg.out_scale = JPEG_IMAGE_SCALE_0;
  cfg.advanced.working_buffer = work;
  cfg.advanced.working_buffer_size = work_size;
  cfg.flags.swap_color_bytes = 1;  // ESP-DL RGB565BE and fmt2jpg use big-endian bytes.
  // Validate all header segment bounds before calling the codec. In
  // particular, do not pass truncated headers to get_image_info().
  if (length < 4 || jpeg[0] != 0xFF || jpeg[1] != 0xD8)
    return JpegDecodeResult::INVALID_JPEG;
  uint16_t width = 0, height = 0;
  bool scan_found = false;
  size_t pos = 2;
  while (pos < length) {
    if (jpeg[pos++] != 0xFF) return JpegDecodeResult::INVALID_JPEG;
    while (pos < length && jpeg[pos] == 0xFF) ++pos;
    if (pos == length) return JpegDecodeResult::INVALID_JPEG;
    const uint8_t marker = jpeg[pos++];
    if (marker == 0 || marker == 0xD8 || marker == 0xD9 || (marker >= 0xD0 && marker <= 0xD7))
      return JpegDecodeResult::INVALID_JPEG;
    if (length - pos < 2) return JpegDecodeResult::INVALID_JPEG;
    const size_t segment_len = (static_cast<size_t>(jpeg[pos]) << 8) | jpeg[pos + 1];
    if (segment_len < 2 || segment_len > length - pos) return JpegDecodeResult::INVALID_JPEG;
    if (marker == 0xC0) {
      if (width != 0 || segment_len < 8 || jpeg[pos + 2] != 8)
        return JpegDecodeResult::INVALID_JPEG;
      const uint8_t channels = jpeg[pos + 7];
      if ((channels != 1 && channels != 3) || segment_len != 8 + 3 * static_cast<size_t>(channels))
        return JpegDecodeResult::INVALID_JPEG;
      height = (static_cast<uint16_t>(jpeg[pos + 3]) << 8) | jpeg[pos + 4];
      width = (static_cast<uint16_t>(jpeg[pos + 5]) << 8) | jpeg[pos + 6];
    } else if (marker >= 0xC1 && marker <= 0xCF && marker != 0xC4) {
      return JpegDecodeResult::INVALID_JPEG;  // baseline MJPEG only
    }
    pos += segment_len;
    if (marker == 0xDA) { scan_found = true; break; }
  }
  if (!scan_found || width == 0 || height == 0) return JpegDecodeResult::INVALID_JPEG;
  const size_t expected = static_cast<size_t>(width) * height * 2;
  if (width > max_width || height > max_height || expected > capacity)
    return JpegDecodeResult::SIZE_LIMIT;
  esp_jpeg_image_output_t info = {};

  cfg.outbuf = rgb;
  cfg.outbuf_size = capacity;
  if (esp_jpeg_decode(&cfg, &info) != ESP_OK || info.width != width || info.height != height ||
      info.output_len != expected)
    return JpegDecodeResult::DECODE_FAILED;
  frame = {width, height, expected};
  return JpegDecodeResult::OK;
}

}  // namespace esphome::usbuvc_version
