# usbuvc_version — USB UVC 鏡頭 AI 辨識

在 ESP32-S3 上執行 ESP-DL 偵測／分類／辨識。影像由既有 `usbuvc` 元件提供：

```text
USB UVC 鏡頭 → usbuvc（MJPEG）→ 獨立 PSRAM JPEG 緩衝
    → 背景解碼 RGB565 → ESP-DL 推論 → 結果／加框 JPEG 回呼
```

本元件來自 [youkorr/s3_vision 的 vision](https://github.com/youkorr/s3_vision/tree/1b599ccf1108e170350b7f1599d09b5b188d8d5c/components/vision)，
來源 commit：`1b599ccf1108e170350b7f1599d09b5b188d8d5c`。
2026-11-11 在本專案改名並將輸入改為 USB UVC MJPEG。
不再使用上游的 DVP `esp32_camera_id`；請改用 `usbuvc_id`。

## 硬體與工具鏈

- **ESP32-S3 + PSRAM**；建議 8MB PSRAM、16MB Flash。ESP32-S2/P4 不在本次 AI 支援範圍。
- 鏡頭須支援 MJPEG；指定的解析度與 FPS 必須由鏡頭宣告支援。
- S3 USB D− 接 GPIO19，D+ 接 GPIO20；鏡頭需穩定的外部 5V，與板子共地。
- 使用 ESP-IDF framework 與 `esp32.toolchain: platformio`。上游 ESP-DL 仍依賴 SCons。
- 已用 ESPHome **2026.10.2** 驗證設定與程式碼產生。該版本提示 PlatformIO
  工具鏈將在 2027.2.0 移除，屆時須另行移植 ESP-DL 建置腳本。

## 設定

完整本機範例見 [example.yaml](example.yaml)。Wi-Fi、API、MQTT 可依需求加入；
範例預設只輸出 UART 日誌。

若從 Git 載入，需等這次修改推送至對應分支後使用：

```yaml
external_components:
  - source: github://sunglee42/myESPhome@work
    components: [usbuvc, usbuvc_version]

usbuvc:
  id: my_webcam
  name: USB Camera
  resolution: 480X320
  fps: 15
  max_framerate: 15 fps
  start_streaming_at_init: true

usbuvc_version:
  id: my_vision
  usbuvc_id: my_webcam
  model_family: human_face_detect
  frame_width: 480
  frame_height: 320
  max_jpeg_size: 262144
  detection_interval_ms: 10000
  on_event:
    - logger.log:
        format: 'Detected %d objects: %s'
        args: [object_count, summary.c_str()]

text_sensor:
  - platform: usbuvc_version
    usbuvc_version_id: my_vision
    detection:
      name: Detection summary

select:
  - platform: usbuvc
    usbuvc_id: my_webcam
    name: Camera resolution and FPS
```

`usbuvc_version` 不管理 USB 驅動或建立第二個鏡頭實體，既有 API／HTTP 原始影像
串流仍由 `usbuvc` 提供。加框 JPEG 透過 `on_detection_image`／`on_augmented_image`
回呼輸出，不會改寫原始串流。

## 影像參數與記憶體

| 參數 | 預設 | 用途 |
|---|---|---|
| `usbuvc_id` | 必填 | 引用既有 UVC 鏡頭 |
| `frame_width` | 480 | 可解碼的最大寬度 |
| `frame_height` | 320 | 可解碼的最大高度 |
| `max_jpeg_size` | 262144 | 單張壓縮 JPEG 的最大位元組數 |
| `detection_interval_ms` | 10000 | 定期請求辨識影像的間隔 |
| `jpeg_quality` | 50 | 加框 JPEG 輸出的品質（1～100） |

**實際寬高從每張 JPEG 標頭讀取**，`frame_width` / `frame_height` 是記憶體上限，
不強制鏡頭解析度，也不進行縮圖。上例可處理 480×320 或寬高均不超過此上限的尺寸；
切到超過上限的格式會記錄警告並略過，需提高上限並重新編譯後才能辨識。

影像緩衝的固定 PSRAM 開銷約為：
`2 × max_jpeg_size + frame_width × frame_height × 2 + 8192` 位元組，
**另加 USB 驅動、模型與推論的記憶體需求**。配置失敗會標記元件失敗，不進行解碼。

回呼內複製壓縮影像後便釋放驅動影像的使用權。單槽待處理佇列保留最新影像，
工作中的影像使用另一個緩衝區；推論與輸出使用同一張已解碼影像。
解碼前檢查 JPEG 標頭、尺寸與容量，使用有輸出容量限制的 Espressif 軟體解碼器。
支援省略 DHT 的標準 MJPEG；不支援 progressive JPEG。

鏡頭擷取 15 FPS 不代表 AI 能執行 15 FPS。軟體 JPEG 解碼與模型推論都需要時間；
建議先以較低解析度、`detection_interval_ms: 10000` 測試。
沒有 Home Assistant 觀看者時，元件也會定期請求影像以執行推論。
`usbuvc` 必須已開始擷取：保留 `start_streaming_at_init: true`，或自行呼叫
`usbuvc.start_stream`。AI 的 start/stop 只控制辨識，不停止其他觀看者的串流。

## 動作、回呼與模型

- 動作：`usbuvc_version.inference`、`.start`、`.stop`、`.enroll`、`.forget`、`.clear_faces`。
  使用字典格式，例如 `usbuvc_version.start: {id: my_vision}`。
- `start` / `stop` 啟用／暫停定期辨識；已在處理的工作可能完成。
- `inference` 請求下一張新影像做一次辨識，暫停時也可使用。
- 保留上游的 `on_event`、`on_object_detected`、`on_classification`、
  `on_recognition`、`on_detection_image` 與 `on_augmented_image`。
- 影像回呼的 `image.data` 僅於該回呼期間有效，需留存時請自行複製。

`_vendor/esp-dl` 為上游附帶的 ESP-DL 原始碼與 S3 模型載入器；
`_vendor/models` 收錄七種家族的預設 S3 模型與授權：
`coco_detect`、`pedestrian_detect`、`hand_detect`、`human_face_detect`、
`coco_pose`、`imagenet_cls`、`hand_gesture_recognition`。
可用 `model_path` 指定 `.espdl`，或用 `model_id` 提供執行期模型。
人臉辨識仍需自行提供偵測模型、特徵模型與資料庫設定。
產生的模型 C 檔與物件檔只寫入建置目錄。

Python 會登記 `espressif/esp32-camera` 2.1.7 作為 **JPEG 轉換函式庫**，
以及 `espressif/esp_jpeg` 1.3.2；不建立 DVP 鏡頭或占用 DVP 腳位。
解碼使用軟體模式、32-bit 最佳化與預設 Huffman 表。

## 驗證

在儲存庫根目錄、啟用 ESPHome Python 環境後執行：

```sh
python -m unittest discover -s components/usbuvc_version/tests -v
esphome config components/usbuvc_version/example.yaml
esphome compile --only-generate components/usbuvc_version/example.yaml
esphome compile components/usbuvc_version/example.yaml
```

建置腳本測試檢查七種預設模型、自訂模型與執行期模型；原生 C++ 測試檢查影像
複製、工作緩衝不被覆寫與待處理影像更新。
設定 `ESP_JPEG_SOURCE_DIR` 為 Espressif `idf-extra-components/esp_jpeg` 1.3.2
原始碼目錄，可額外執行實際 JPEG 解碼器的 ASan／UBSan 測試（需 Pillow、gcc/g++）：

```sh
ESP_JPEG_SOURCE_DIR=/path/to/idf-extra-components/esp_jpeg \
  python -m unittest discover -s components/usbuvc_version/tests -v
```

測試包含 480×320、320×240、缺少 DHT、RGB565 色彩順序、解析度／容量上限與
截斷影像。主機測試以 shim 代替 ESP 系統配置／記憶體介面，並在暫存副本中將
解碼器輸入回呼的 `unsigned int` 改為 `size_t`，以符合 64-bit 主機 ABI；
JPEG 解碼邏輯仍使用實際 Espressif 程式碼。

本次上述設定、程式碼產生及五項測試通過。
完整 ESP32 韌體建置仍受工具鏈套件下載存取限制，尚未完成；
**尚無實體 UVC 鏡頭或板子的端到端驗證**。

## 來源與授權

保留上游各檔案的著作權與 SPDX 標示。
上游 C/C++ 為 **GPL-3.0-or-later**，Python 整合／建置腳本為 MIT；
附帶 ESP-DL 與所選模型保留各自 MIT 授權。
[LICENSE](LICENSE) 與 [LICENSE-MIT](LICENSE-MIT) 提供條款全文，
本專案根目錄的 MIT 不取代這些第三方授權。
