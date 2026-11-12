#include "usbuvc_enum.h"
#include "esphome/core/log.h"
#include "esp_intr_alloc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstdio>

namespace esphome {
namespace usbuvc_enum {
static const char *const TAG = "uvc_enum";
static uint16_t le16(const uint8_t *p) { return p[0] | (uint16_t(p[1]) << 8); }
static uint32_t le32(const uint8_t *p) { return le16(p) | (uint32_t(le16(p + 2)) << 16); }

void UvcEnumerator::setup() {
  usb_host_config_t config = {};
  config.intr_flags = ESP_INTR_FLAG_LOWMED;
  esp_err_t err = usb_host_install(&config);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "usb_host_install: %s", esp_err_to_name(err));
    this->mark_failed();
    return;
  }
  if (xTaskCreate(library_task_, "uvc_enum_lib", 4096, this, 15, nullptr) != pdPASS) {
    ESP_LOGE(TAG, "USB library task allocation failed");
    usb_host_uninstall();
    this->mark_failed();
    return;
  }
  usb_host_client_config_t client_config = {};
  client_config.is_synchronous = false;
  client_config.max_num_event_msg = 5;
  client_config.async.client_event_callback = client_event_;
  client_config.async.callback_arg = this;
  err = usb_host_client_register(&client_config, &this->client_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "usb_host_client_register: %s", esp_err_to_name(err));
    this->mark_failed();
    return;
  }
  if (xTaskCreate(client_task_, "uvc_enum_client", 6144, this, 5, nullptr) != pdPASS) {
    ESP_LOGE(TAG, "USB client task allocation failed");
    usb_host_client_deregister(this->client_);
    this->mark_failed();
    return;
  }
  ESP_LOGI(TAG, "Descriptor enumeration only. No PROBE/COMMIT or video transfers.");
  ESP_LOGI(TAG, "ESP32-S3 USB Full-Speed: D-=GPIO19 D+=GPIO20. Waiting for camera...");
}

void UvcEnumerator::library_task_(void *) {
  while (true) {
    uint32_t flags = 0;
    usb_host_lib_handle_events(portMAX_DELAY, &flags);
    if (flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS)
      usb_host_device_free_all();
  }
}

void UvcEnumerator::client_event_(const usb_host_client_event_msg_t *event, void *arg) {
  auto *self = static_cast<UvcEnumerator *>(arg);
  if (event->event == USB_HOST_CLIENT_EVENT_NEW_DEV)
    self->pending_address_ = event->new_dev.address;
  else if (event->event == USB_HOST_CLIENT_EVENT_DEV_GONE &&
           event->dev_gone.dev_hdl == self->device_)
    self->disconnected_ = true;
}

void UvcEnumerator::client_task_(void *arg) {
  auto *self = static_cast<UvcEnumerator *>(arg);
  while (true) {
    usb_host_client_handle_events(self->client_, pdMS_TO_TICKS(100));
    if (self->disconnected_) {
      usb_host_device_close(self->client_, self->device_);
      self->device_ = nullptr;
      self->disconnected_ = false;
      ESP_LOGI(TAG, "Device disconnected. Waiting for reconnect...");
    }
    if (self->pending_address_ != 0) {
      const uint8_t address = self->pending_address_;
      self->pending_address_ = 0;
      if (self->device_ == nullptr)
        self->inspect_(address);
    }
  }
}

void UvcEnumerator::inspect_(uint8_t address) {
  esp_err_t err = usb_host_device_open(this->client_, address, &this->device_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "device_open: %s", esp_err_to_name(err));
    return;
  }
  const usb_device_desc_t *desc = nullptr;
  err = usb_host_get_device_descriptor(this->device_, &desc);
  if (err == ESP_OK) {
    ESP_LOGI(TAG, "=== Device address=%u VID=0x%04X PID=0x%04X ===", address, desc->idVendor, desc->idProduct);
    ESP_LOGI(TAG, "bcdUSB=0x%04X bcdDevice=0x%04X class=0x%02X subclass=0x%02X protocol=0x%02X",
             desc->bcdUSB, desc->bcdDevice, desc->bDeviceClass, desc->bDeviceSubClass, desc->bDeviceProtocol);
    ESP_LOGI(TAG, "EP0 packet=%u configurations=%u string indices: manufacturer=%u product=%u serial=%u",
             desc->bMaxPacketSize0, desc->bNumConfigurations, desc->iManufacturer, desc->iProduct, desc->iSerialNumber);
  } else {
    ESP_LOGE(TAG, "device_descriptor: %s", esp_err_to_name(err));
  }
  const usb_config_desc_t *config = nullptr;
  err = usb_host_get_active_config_descriptor(this->device_, &config);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "config_descriptor: %s", esp_err_to_name(err));
    return;
  }
  ESP_LOGI(TAG, "Active configuration=%u interfaces=%u total_length=%u max_power=%u mA attributes=0x%02X",
           config->bConfigurationValue, config->bNumInterfaces, config->wTotalLength,
           unsigned(config->bMaxPower) * 2, config->bmAttributes);
  this->parse_(reinterpret_cast<const uint8_t *>(config), config->wTotalLength);
}

void UvcEnumerator::parse_(const uint8_t *data, size_t length) {
  uint8_t cls = 0, sub = 0, format_index = 0;
  bool video = false, uncompressed = false;
  for (size_t offset = 0; offset + 2 <= length;) {
    const uint8_t *d = data + offset;
    const size_t n = d[0];
    if (n < 2 || n > length - offset) {
      ESP_LOGE(TAG, "Malformed descriptor at offset=%u length=%u", unsigned(offset), unsigned(n));
      return;
    }
    // Preserve every descriptor, including unsupported/vendor-specific fields.
    for (size_t row = 0; row < n; row += 16) {
      char hex[49] = {};
      const size_t count = n - row < 16 ? n - row : 16;
      for (size_t i = 0; i < count; i++)
        std::snprintf(hex + i * 3, sizeof(hex) - i * 3, "%02X ", d[row + i]);
      ESP_LOGD(TAG, "RAW offset=%u: %s", unsigned(offset + row), hex);
    }
    if (d[1] == 0x04 && n >= 9) {
      cls = d[5];
      sub = d[6];
      format_index = 0;
      video |= cls == 0x0E;
      ESP_LOGI(TAG, "Interface=%u alt=%u endpoints=%u class=0x%02X subclass=0x%02X protocol=0x%02X",
               d[2], d[3], d[4], cls, sub, d[7]);
    } else if (d[1] == 0x05 && n >= 7) {
      static const char *const types[] = {"control", "isochronous", "bulk", "interrupt"};
      const uint16_t packet = le16(d + 4);
      ESP_LOGI(TAG, "Endpoint=0x%02X direction=%s type=%s wMaxPacketSize=0x%04X payload=%u interval=%u",
               d[2], (d[2] & 0x80) ? "IN" : "OUT", types[d[3] & 3], packet, packet & 0x7FF, d[6]);
    } else if (d[1] == 0x24 && n >= 3 && cls == 0x0E) {
      const uint8_t subtype = d[2];
      if (sub == 1 && subtype == 1 && n >= 12) {
        ESP_LOGI(TAG, "VideoControl: bcdUVC=0x%04X total_length=%u clock=%u Hz",
                 le16(d + 3), le16(d + 5), unsigned(le32(d + 7)));
      } else if (sub == 2 && subtype == 1 && n >= 7) {
        ESP_LOGI(TAG, "VideoStreaming input: formats=%u total_length=%u endpoint=0x%02X", d[3], le16(d + 4), d[6]);
      } else if (sub == 2 && subtype == 4 && n >= 27) {
        format_index = d[3];
        uncompressed = true;
        ESP_LOGI(TAG, "UNCOMPRESSED format=%u frames=%u bits_per_pixel=%u default_frame=%u",
                 format_index, d[4], d[21], d[22]);
        char guid[49] = {};
        for (size_t i = 0; i < 16; i++)
          std::snprintf(guid + i * 3, sizeof(guid) - i * 3, "%02X ", d[5 + i]);
        ESP_LOGI(TAG, "  GUID bytes: %s", guid);
        if (d[5] >= 32 && d[5] <= 126 && d[6] >= 32 && d[6] <= 126 &&
            d[7] >= 32 && d[7] <= 126 && d[8] >= 32 && d[8] <= 126)
          ESP_LOGI(TAG, "  GUID prefix/FourCC candidate: %c%c%c%c", d[5], d[6], d[7], d[8]);
      } else if (sub == 2 && subtype == 6 && n >= 11) {
        format_index = d[3];
        ESP_LOGI(TAG, "MJPEG format=%u frames=%u default_frame=%u", format_index, d[4], d[6]);
      } else if (sub == 2 && (subtype == 5 || subtype == 7) && n >= 26) {
        ESP_LOGI(TAG, "%s frame: format=%u index=%u %ux%u bit_rate=%u..%u bit/s buffer=%u bytes",
                 subtype == 5 ? "UNCOMPRESSED" : "MJPEG", format_index, d[3], le16(d + 5), le16(d + 7),
                 unsigned(le32(d + 9)), unsigned(le32(d + 13)), unsigned(le32(d + 17)));
        const uint32_t interval = le32(d + 21);
        ESP_LOGI(TAG, "  default interval=%u x100ns (%.3f fps)", unsigned(interval), interval ? 10000000.0 / interval : 0.0);
        if (d[25] == 0 && n >= 38) {
          ESP_LOGI(TAG, "  continuous interval min=%u max=%u step=%u x100ns",
                   unsigned(le32(d + 26)), unsigned(le32(d + 30)), unsigned(le32(d + 34)));
        } else if (d[25] != 0) {
          if (n < 26U + 4U * d[25])
            ESP_LOGW(TAG, "  Truncated frame interval list");
          for (size_t i = 0; i < d[25] && 26 + i * 4 + 4 <= n; i++) {
            const uint32_t value = le32(d + 26 + i * 4);
            ESP_LOGI(TAG, "  interval=%u x100ns => %.3f fps", unsigned(value), value ? 10000000.0 / value : 0.0);
          }
        }
      } else {
        ESP_LOGI(TAG, "UVC class descriptor: subclass=0x%02X subtype=0x%02X length=%u (see RAW)", sub, subtype, unsigned(n));
      }
    }
    offset += n;
  }
  ESP_LOGI(TAG, "=== Enumeration complete: UVC=%s uncompressed_descriptor=%s ===",
           video ? "yes" : "no", uncompressed ? "yes" : "not found");
  ESP_LOGI(TAG, "Active configuration at S3 Full-Speed only. Advertised modes are not a streaming test.");
}
}  // namespace usbuvc_enum
}  // namespace esphome
