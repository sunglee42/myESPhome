#pragma once
#include "esphome/core/component.h"
#include "usb/usb_host.h"

namespace esphome {
namespace usbuvc_enum {
class UvcEnumerator : public Component {
 public:
  void setup() override;
 protected:
  static void library_task_(void *arg);
  static void client_task_(void *arg);
  static void client_event_(const usb_host_client_event_msg_t *event, void *arg);
  void inspect_(uint8_t address);
  void parse_(const uint8_t *data, size_t length);
  usb_host_client_handle_t client_{nullptr};
  usb_device_handle_t device_{nullptr};
  uint8_t pending_address_{0};
  bool disconnected_{false};
};
}  // namespace usbuvc_enum
}  // namespace esphome
