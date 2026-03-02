# Multicast-Hamming-File-Transfer
這是一個基於 C 語言開發的可靠檔案傳輸系統。本專案利用 **UDP Multicast (組播)** 技術同時向多個用戶端傳送檔案，並實作了 **Hamming Code** 作為前向錯誤更正（FEC）機制，以確保在網路傳輸中發生位元錯誤時仍能維持資料正確性。



## 🌟 核心特點

* **多點傳送效率**：使用 UDP Multicast 同時服務至少 3 個 Client，無需為每個接收端重複發送數據。
* **前向錯誤更正 (FEC)**：實作 $12$-bit Hamming Code 來編碼 8-bit 的原始資料，容許接收端在不要求重傳的情況下修正 1-bit 的錯誤。
* **封包損耗偵測**：透過在封包標頭（前 12 bytes）加入序列號，接收端可根據編號連續性自動計算封包丟失率（Packet drop rate)。
* **流量控制**：發送端加入 `usleep(1)` 延遲機制，有效降低因發送過快導致的封包掉落。

## 🛠 技術實作細節

### 數據封裝流程
1. **資料讀取**：Server 以二進位模式開啟檔案，每次讀取 500 bytes 數據。
2. **標頭附加**：在緩衝區前 12 bytes 寫入封包編號 。
3. **Hamming 編碼**：將 512 bytes 的 Buffer 編碼為 1024 bytes 的 `send_buffer`。
4. **解碼恢復**：Client 接收後進行解碼，並根據序列號判斷是否結束傳輸。

### 網路參數
* **傳輸協定**：UDP (User Datagram Protocol)。
* **預設組播位址**：`226.1.1.1`。
* **預設連接埠**：`8888`。

## 💻 執行說明

### 編譯環境
使用 `gcc` 在 Linux 環境下編譯：
```bash
gcc multicast_server.c -o server
gcc multicast_client.c -o client
