# ESP32-S3 UVC 描述符枚舉

搭配 `code/usbuvc_s3_enumerate.yaml` 使用。此元件獨立於 `usbuvc`，不開啟串流、不執行 UVC PROBE/COMMIT，也不宣告 ESPHome camera 實體。

## 執行

設定檔會從 `github://sunglee42/myESPhome@work` 載入元件。於專案根目錄的 ESPHome 環境執行：

```sh
esphome run code/usbuvc_s3_enumerate.yaml --device /dev/ttyUSB0
# 已燒錄時，僅監看：
esphome logs code/usbuvc_s3_enumerate.yaml --device /dev/ttyUSB0
```

使用實際序列埠名稱取代 `/dev/ttyUSB0`。鏡頭 D− 接 GPIO19、D+ 接 GPIO20，外部供應 5V 並共地。UART0 日誌預設 TX=GPIO43 / RX=GPIO44，使用獨立 USB-UART；不要將 5V 接至 GPIO。

## 日誌判讀

- `VID`、`PID`、USB／裝置版本、介面、alternate setting、端點類型與封包大小。
- VideoControl 的 UVC 版本、VideoStreaming 格式資訊。
- `UNCOMPRESSED`：描述符宣告非壓縮格式；輸出完整 GUID 及可列印的 FourCC 前綴，例如 YUY2。
- `MJPEG`：描述符宣告 MJPEG 壓縮格式。
- 兩類 frame descriptor 顯示解析度、資料率、buffer 大小與 frame intervals（100ns 單位）；離散 interval 同時換算 fps。
- `RAW`：目前組態的所有原始描述符，包含尚未解碼的控制項與其他格式。
- 結尾 `uncompressed_descriptor=yes` 只代表當前組態找到標準非壓縮格式描述符。`not found` 不代表鏡頭在其他速度／組態也不支援。

只讀取 ESP32-S3 Full-Speed 連線下的 active configuration。未讀取其他 configuration、High-Speed 模式或各控制項的目前值／範圍。描述符宣告模式不保證 S3 可實際收流。

## 驗證狀態

ESPHome 2026.10.2 設定檢查、C++ 程式碼產生成功。解析器以合成 YUY2 描述符確認格式、解析度、fps，並通過截斷資料及 10,000 組隨機／畸形資料的 AddressSanitizer、UndefinedBehaviorSanitizer 檢查。未完成韌體編譯及實機測試。
