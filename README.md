# ExpressLRS Sender

Raspberry Pi + BETAFPV NANO TX V2 を使用して、事前に記録した操作履歴をドローンに送信するシステム。

## 概要

```
[Raspberry Pi] --UART/CRSF--> [BETAFPV NANO TX V2] ~~~RF~~~ [ExpressLRS RX] --> [FC]
```

- CRSFプロトコルでTXモジュールと通信
- CSV/JSON形式の操作履歴を再生
- 安全機能（Armインターロック、Failsafe）搭載

## 必要なハードウェア

- Raspberry Pi 4/5
- BETAFPV NANO TX モジュール V2（ExpressLRS 2.4GHz）
- ExpressLRS対応受信機を搭載したドローン

## 接続

NANO TX V2 の CRSF 通信は S.Port ピン1本によるハーフデュプレックスです。

| Raspberry Pi | NANO TX V2 |
|-------------|------------|
| GPIO14 (TXD) | S.Port (Signal) |
| 5V | VCC |
| GND | GND |

テレメトリ受信が必要な場合は GPIO15 (RXD) も S.Port に接続しますが、半二重のためソフトウェア側での送受信制御が必要です。

## GPIO/UART の選択

Raspberry Pi 4/5 では複数の UART を利用できます。`--gpio` オプションまたは `gpio` サブコマンドで GPIO ピンと UART の対応を確認・指定できます。

### 利用可能な UART-GPIO マッピング

| UART | GPIO TX | GPIO RX | デバイス | 備考 |
|------|---------|---------|---------|------|
| UART0 | 14 | 15 | /dev/ttyAMA0 | デフォルト（PL011） |
| UART2 | 0 | 1 | /dev/ttyAMA1 | I2C0 と共有 |
| UART3 | 4 | 5 | /dev/ttyAMA2 | |
| UART4 | 8 | 9 | /dev/ttyAMA3 | SPI0 CE0/CE1 と共有 |
| UART5 | 12 | 13 | /dev/ttyAMA4 | |

UART1（mini UART）は 420000 baud を安定してサポートできないため除外しています。

### UART の有効化

追加の UART を使用するには `/boot/config.txt`（Pi 5 では `/boot/firmware/config.txt`）に dtoverlay を追加します:

```
dtoverlay=uart3
dtoverlay=uart4
dtoverlay=uart5
```

再起動後に有効になります。

### GPIO ピン指定での接続

```bash
# GPIO4 (UART3) を使用
sudo ./expresslrs_sender --gpio 4 play -H data/flight.csv

# GPIO12 (UART5) を使用
sudo ./expresslrs_sender --gpio 12 ping
```

### マッピングテーブルの表示

```bash
./expresslrs_sender gpio
```

### 設定ファイルでの指定

`config/default.json` で `gpio_tx` を指定すると、`port` の代わりに GPIO ピン番号から自動的にデバイスパスが解決されます:

```json
{
  "device": {
    "gpio_tx": 4,
    "baudrate": 420000
  }
}
```

### 注意事項

- UART2 (GPIO0/1) は I2C0 とピンを共有しています。I2C を使用する場合は避けてください
- UART4 (GPIO8/9) は SPI0 CE0/CE1 とピンを共有しています
- `--gpio` オプションと `-d`/`--device` を同時に指定した場合、後に指定した方が優先されます

## 依存ライブラリのインストール

### Raspberry Pi (Debian/Ubuntu)

```bash
sudo apt update
sudo apt install -y cmake build-essential
sudo apt install -y nlohmann-json3-dev libspdlog-dev libgtest-dev
```

### macOS (開発用)

```bash
brew install cmake nlohmann-json spdlog googletest
```

## ビルド

```bash
git clone <repository-url>
cd expresslrs_sender

mkdir build && cd build
cmake ..
make -j4
```

### デバッグビルド

```bash
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j4
```

### テストを無効化してビルド

```bash
cmake -DENABLE_TESTS=OFF ..
make -j4
```

## テスト

```bash
cd build
ctest --output-on-failure
```

### 詳細出力

```bash
ctest -V
```

### 特定のテストのみ実行

```bash
./test_expresslrs_sender --gtest_filter="CrsfTest.*"
```

### カバレッジ付きビルド

```bash
cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_COVERAGE=ON ..
make -j4
ctest
lcov --capture --directory . --output-file coverage.info
genhtml coverage.info --output-directory coverage_report
```

## インストール

```bash
sudo make install
```

デフォルトでは `/usr/local/bin/expresslrs_sender` にインストールされます。

## Raspberry Pi の設定

### シリアルコンソールの無効化

```bash
sudo raspi-config
# Interface Options → Serial Port → No (login shell) → Yes (hardware)
sudo reboot
```

### UART権限

```bash
sudo usermod -a -G dialout $USER
# 再ログインが必要
```

## 使い方

### ヘルプ

```bash
./expresslrs_sender --help
```

### 操作履歴の再生

```bash
sudo ./expresslrs_sender play -H data/sample.csv
```

### ループ再生

```bash
sudo ./expresslrs_sender play -H data/flight.csv --loop
```

### 2倍速再生

```bash
sudo ./expresslrs_sender play -H data/flight.csv --speed 2.0
```

### ドライラン（送信なし）

```bash
./expresslrs_sender play -H data/sample.csv --dry-run -v
```

### 操作履歴の検証

```bash
./expresslrs_sender validate -H data/sample.csv
```

### TXモジュールとの接続確認

```bash
sudo ./expresslrs_sender ping
```

### デバイス情報の表示

```bash
sudo ./expresslrs_sender info
```

### 設定ファイルの指定

```bash
sudo ./expresslrs_sender -c config/custom.json play -H data/flight.csv
```

## 設定ファイル

`config/default.json`:

```json
{
  "device": {
    "port": "/dev/ttyAMA0",
    "baudrate": 420000,
    "invert_tx": false,
    "invert_rx": false,
    "gpio_tx": -1
  },
  "playback": {
    "default_rate_hz": 50,
    "arm_delay_ms": 3000
  },
  "safety": {
    "arm_channel": 5,
    "arm_threshold": 1500,
    "throttle_min": 172,
    "failsafe_timeout_ms": 500,
    "disarm_frames": 10
  },
  "logging": {
    "level": "info"
  }
}
```

## 操作履歴ファイル形式

### CSV形式

```csv
timestamp_ms,ch1,ch2,ch3,ch4,ch5,ch6,ch7,ch8
0,992,992,172,992,172,172,172,172
20,992,992,200,992,172,172,172,172
40,992,992,300,992,172,172,172,172
```

### JSON形式

```json
{
  "metadata": {
    "name": "test_flight",
    "duration_ms": 60000
  },
  "frames": [
    {"t": 0, "ch": [992, 992, 172, 992, 172, 172, 172, 172]},
    {"t": 20, "ch": [992, 992, 200, 992, 172, 172, 172, 172]}
  ]
}
```

### チャンネルマッピング

| チャンネル | 機能 | CRSF値 |
|-----------|------|--------|
| CH1 | Roll | 172-1811 |
| CH2 | Pitch | 172-1811 |
| CH3 | Throttle | 172-1811 |
| CH4 | Yaw | 172-1811 |
| CH5 | Arm | 172=OFF, 1811=ON |
| CH6-16 | Aux | 172-1811 |

## 安全機能

- **Arm インターロック**: Armスイッチが有効になるまでThrottleは最小値に固定
- **Arm遅延**: Arm要求から3秒後に有効化（設定変更可能）
- **Failsafe**: 通信途絶時は自動的にDisarm
- **緊急停止**: Ctrl+C で即座にDisarm状態で終了

## トラブルシューティング

### UART デバイスが見つからない

```bash
ls -la /dev/ttyAMA0 /dev/serial0
```

### Permission denied

```bash
sudo usermod -a -G dialout $USER
# 再ログイン後に再試行
```

### TXモジュールと通信できない

1. 接続を確認（GPIO TX が S.Port ピンに正しく接続されているか）
2. 信号反転が必要な場合は設定で `"invert_tx": true`
3. ボーレートが正しいか確認（420000 bps）

### ボーレートエラー

Raspberry Pi で 420000 bps がサポートされていない場合:
```bash
# /boot/config.txt に追加
init_uart_clock=64000000
```

## ライセンス

MIT License

## 参考

- [ExpressLRS Documentation](https://www.expresslrs.org/)
- [CRSF Protocol](https://github.com/ExpressLRS/ExpressLRS/wiki/CRSF-Protocol)
- [BETAFPV NANO TX Module](https://betafpv.com/products/elrs-nano-tx-module)
