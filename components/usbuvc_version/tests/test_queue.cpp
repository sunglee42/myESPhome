#include "usbuvc_version_frame_queue.h"
#include <array>
#include <cassert>
using namespace esphome::usbuvc_version;
int main() {
  std::array<uint8_t, 4> a{}, b{};
  JpegFrameQueue q;
  q.setup(a.data(), b.data(), a.size());
  const uint8_t *data = nullptr;
  size_t length = 0;
  assert(!q.take(data, length));
  uint8_t source[] = {1, 2, 3};
  assert(q.push(source, 3));
  source[0] = 99;  // A USB driver may immediately reuse the source buffer.
  assert(q.take(data, length) && length == 3 && data[0] == 1);
  const uint8_t *in_flight = data;
  assert(q.push(source, 3));
  source[0] = 42;
  assert(q.push(source, 3));  // Latest pending frame replaces stale work.
  assert(in_flight[0] == 1);  // Producer cannot overwrite worker's frame.
  assert(!q.push(source, 5));
  assert(!q.push(nullptr, 1));
  assert(!q.push(source, 0));
  assert(q.take(data, length) && data[0] == 42 && length == 3);
  assert(!q.take(data, length));
}
