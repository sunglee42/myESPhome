#include "usbuvc_version_jpeg.h"
#include <cassert>
#include <fstream>
#include <iterator>
#include <vector>
using namespace esphome::usbuvc_version;
int main(int argc, char **argv) {
  assert(argc == 4);
  std::vector<uint8_t> output(640 * 480 * 2 + 32, 0xA5);
  uint8_t *rgb = output.data() + 16;
  const size_t capacity = output.size() - 32;
  DecodedFrame info;
  std::vector<uint8_t> work(8192);
  auto decode = [&](const uint8_t *jpeg, size_t length, uint8_t *rgb, size_t capacity,
                    uint16_t width, uint16_t height, DecodedFrame &frame) {
    return decode_jpeg(jpeg, length, rgb, capacity, width, height, work.data(), work.size(), frame);
  };
  for (int i = 1; i < argc; ++i) {
    std::ifstream file(argv[i], std::ios::binary);
    std::vector<uint8_t> jpeg((std::istreambuf_iterator<char>(file)), {});
    assert(!jpeg.empty());
    assert(decode(jpeg.data(), jpeg.size(), rgb, capacity, 640, 480, info) == JpegDecodeResult::OK);
    assert(info.width == (i == 2 ? 320 : 480) && info.height == (i == 2 ? 240 : 320));
    assert(info.length == static_cast<size_t>(info.width) * info.height * 2);
    const uint16_t red = (static_cast<uint16_t>(rgb[0]) << 8) | rgb[1];
    assert((red >> 11) >= 28 && ((red >> 5) & 63) < 4 && (red & 31) < 4); // RGB565BE
    assert(decode(jpeg.data(), jpeg.size(), rgb, capacity, 160, 120, info) == JpegDecodeResult::SIZE_LIMIT);
    assert(info.length == 0);
    assert(decode(jpeg.data(), jpeg.size(), rgb, 2, 640, 480, info) == JpegDecodeResult::SIZE_LIMIT);
    // Every truncated header must fail safely; later truncated entropy may
    // still be decodable, but may never escape the output buffer.
    for (size_t n = 0; n < jpeg.size(); ++n)
      decode(jpeg.data(), n, rgb, capacity, 640, 480, info);
    for (size_t n = 0; n < 16; ++n) {
      assert(output[n] == 0xA5);
      assert(output[output.size() - 1 - n] == 0xA5);
    }
  }
  const uint8_t malformed[] = {0xFF, 0xD8, 0xFF, 0xE0, 0xFF, 0xFF};
  assert(decode(malformed, sizeof(malformed), rgb, capacity, 640, 480, info) == JpegDecodeResult::INVALID_JPEG);
  assert(decode(nullptr, 0, rgb, capacity, 640, 480, info) == JpegDecodeResult::INVALID_JPEG);
  assert(info.length == 0);
}
