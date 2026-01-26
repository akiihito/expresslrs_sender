# ExpressLRS Sender 仕様設計書

## 概要

Raspberry Pi に接続した BETAFPV NANO TX モジュール V2 を通じて、事前に記録された操作履歴をドローンに送信するシステム。

## システム構成

```
[Raspberry Pi] --UART/CRSF--> [BETAFPV NANO TX V2] ~~~RF~~~ [ExpressLRS RX] --> [FC]
```

### ハードウェア

| コンポーネント | 説明 |
|--------------|------|
| Raspberry Pi | メイン制御ボード（Pi 4/5 推奨） |
| BETAFPV NANO TX V2 | ExpressLRS 2.4GHz 送信モジュール |
| 接続方式 | UART (CRSF プロトコル) |

### 接続ピンアサイン

| Raspberry Pi | NANO TX V2 |
|-------------|------------|
| GPIO14 (TXD) | RX |
| GPIO15 (RXD) | TX |
| 5V | VCC |
| GND | GND |

### 接続時の注意事項

1. **シリアルコンソールの無効化**: Raspberry Piのシリアルコンソールを無効化する必要があります。
   ```bash
   sudo raspi-config
   # Interface Options → Serial Port → No (login shell) → Yes (hardware)
   ```

2. **デバイスアドレス**: TXモジュールに送信する場合は `0xEE` を使用します（FC宛ては `0xC8`）。

3. **信号反転**: 一部のモジュールや接続方法では、UART TX信号の論理反転が必要な場合があります。
   - 設定ファイルで `"invert_tx": true` を設定
   - または、ハードウェアインバーターを使用

4. **ボーレート**: 420000 bps は非標準のため、Raspberry Piのデバイスツリーオーバーレイで対応が必要な場合があります。

## 通信仕様

### CRSF (Crossfire) プロトコル

- **ボーレート**: 420000 bps (CRSF 標準)
- **データビット**: 8
- **パリティ**: なし
- **ストップビット**: 1
- **フロー制御**: なし

### CRSF フレーム構造

```
[Sync] [Length] [Type] [Payload...] [CRC8]
```

| フィールド | サイズ | 説明 |
|-----------|--------|------|
| Sync | 1 byte | デバイスアドレス（宛先による） |
| Length | 1 byte | Type + Payload + CRC のバイト数 |
| Type | 1 byte | フレームタイプ |
| Payload | N bytes | データ本体 |
| CRC8 | 1 byte | CRC-8 DVB-S2 |

### デバイスアドレス

| アドレス | 用途 |
|---------|------|
| 0xC8 | FC（Flight Controller）宛て |
| 0xEE | TXモジュール宛て（Raspberry Pi → TX で使用） |
| 0xEA | RXモジュール宛て |

### 主要フレームタイプ

| Type | 値 | 説明 |
|------|-----|------|
| RC_CHANNELS_PACKED | 0x16 | RCチャンネルデータ (16ch) |
| LINK_STATISTICS | 0x14 | リンク統計情報 |
| DEVICE_PING | 0x28 | デバイス探索 |

### RC チャンネルデータ形式

- 16チャンネル × 11ビット = 176ビット = 22バイト
- 値の範囲: 172 〜 1811 (CRSF標準)
- 中央値: 992
- マッピング: 988〜2012 us → 172〜1811

## 機能要件

### 1. 操作履歴再生機能

#### 入力形式
操作履歴は CSV または JSON 形式で保存:

**CSV形式:**
```csv
timestamp_ms,ch1,ch2,ch3,ch4,ch5,ch6,ch7,ch8
0,992,992,172,992,172,172,172,172
20,992,992,200,992,172,172,172,172
40,992,992,250,992,172,172,172,172
```

**JSON形式:**
```json
{
  "metadata": {
    "name": "test_flight_01",
    "duration_ms": 60000,
    "packet_rate_hz": 50
  },
  "frames": [
    {"t": 0, "ch": [992, 992, 172, 992, 172, 172, 172, 172]},
    {"t": 20, "ch": [992, 992, 200, 992, 172, 172, 172, 172]}
  ]
}
```

#### チャンネルマッピング（標準）

| チャンネル | 機能 | 備考 |
|-----------|------|------|
| CH1 | Roll (Aileron) | |
| CH2 | Pitch (Elevator) | |
| CH3 | Throttle | 最小値 = disarm |
| CH4 | Yaw (Rudder) | |
| CH5 | Arm スイッチ | |
| CH6 | Flight Mode | |
| CH7 | Aux 1 | |
| CH8 | Aux 2 | |

### 2. パケット送信制御

- **送信レート**: 50Hz 〜 500Hz（ExpressLRS設定に依存）
- **タイミング精度**: ±1ms 以内
- **リアルタイム優先度**: SCHED_FIFO で高優先度実行

### 3. 安全機能

- **Failsafe**: 通信途絶時は自動的に安全な値を送信
- **Arm インターロック**: 明示的な Arm コマンドがない限り Throttle を最小に固定
- **緊急停止**: SIGINT/SIGTERM で即座に Disarm 状態へ移行

## 非機能要件

### パフォーマンス
- CPU使用率: 10% 以下（Raspberry Pi 4 基準）
- メモリ使用量: 50MB 以下
- 起動時間: 3秒以内

### 信頼性
- 連続稼働: 24時間以上
- パケットロス: 0%（UART区間）

## ソフトウェアアーキテクチャ

```
┌─────────────────────────────────────────────────────────┐
│                    Main Application                      │
├──────────────┬──────────────┬──────────────┬────────────┤
│  Config      │  History     │  Playback    │  Safety    │
│  Manager     │  Loader      │  Controller  │  Monitor   │
├──────────────┴──────────────┴──────────────┴────────────┤
│                    CRSF Protocol Layer                   │
├─────────────────────────────────────────────────────────┤
│                    UART Driver                           │
└─────────────────────────────────────────────────────────┘
```

### モジュール構成

| モジュール | 責務 |
|-----------|------|
| Config Manager | 設定ファイルの読み込み・管理 |
| History Loader | 操作履歴ファイルの読み込み・パース |
| Playback Controller | タイミング制御・再生状態管理 |
| Safety Monitor | 安全機能の監視・制御 |
| CRSF Protocol Layer | CRSFフレームの構築・解析 |
| UART Driver | シリアル通信の低レベル制御 |

## ディレクトリ構成

```
expresslrs_sender/
├── SPECIFICATION.md      # 本ドキュメント
├── CLAUDE.md            # Claude Code 設定
├── CMakeLists.txt       # ビルド設定
├── README.md            # プロジェクト説明
├── src/
│   ├── main.cpp
│   ├── config/
│   │   ├── config.hpp
│   │   └── config.cpp
│   ├── crsf/
│   │   ├── crsf.hpp
│   │   ├── crsf.cpp
│   │   └── crc8.cpp
│   ├── uart/
│   │   ├── uart.hpp
│   │   └── uart.cpp
│   ├── history/
│   │   ├── history_loader.hpp
│   │   └── history_loader.cpp
│   ├── playback/
│   │   ├── playback_controller.hpp
│   │   └── playback_controller.cpp
│   └── safety/
│       ├── safety_monitor.hpp
│       └── safety_monitor.cpp
├── include/
│   └── expresslrs_sender/
│       └── types.hpp
├── config/
│   └── default.json     # デフォルト設定
├── data/
│   └── sample.csv       # サンプル操作履歴
└── tests/
    ├── test_crsf.cpp
    ├── test_history.cpp
    └── test_playback.cpp
```

## ビルド・実行環境

### 依存ライブラリ
- CMake >= 3.16
- GCC >= 9.0 (C++17 サポート)
- nlohmann/json (JSON パース)
- spdlog (ログ出力)

### ビルド手順
```bash
mkdir build && cd build
cmake ..
make -j4
```

### 実行
```bash
sudo ./expresslrs_sender play --history data/flight.csv
```

## コマンドライン仕様

### 基本構文

```
expresslrs_sender [グローバルオプション] <コマンド> [コマンドオプション]
```

### グローバルオプション

| オプション | 短縮 | 説明 | デフォルト |
|-----------|------|------|-----------|
| `--config <file>` | `-c` | 設定ファイルパス | `config/default.json` |
| `--device <path>` | `-d` | UARTデバイスパス | `/dev/ttyAMA0` |
| `--baudrate <bps>` | `-b` | ボーレート | `420000` |
| `--verbose` | `-v` | 詳細ログ出力 | off |
| `--quiet` | `-q` | エラーのみ出力 | off |
| `--help` | `-h` | ヘルプ表示 | - |
| `--version` | `-V` | バージョン表示 | - |

### コマンド一覧

#### `play` - 操作履歴の再生

操作履歴ファイルを読み込み、ドローンに送信する。

```bash
expresslrs_sender play [オプション] --history <file>
```

| オプション | 短縮 | 説明 | デフォルト |
|-----------|------|------|-----------|
| `--history <file>` | `-H` | 操作履歴ファイル（必須） | - |
| `--rate <hz>` | `-r` | パケット送信レート | `50` |
| `--loop` | `-l` | ループ再生 | off |
| `--loop-count <n>` | | ループ回数（0=無限） | `0` |
| `--start-time <ms>` | | 開始位置（ミリ秒） | `0` |
| `--end-time <ms>` | | 終了位置（ミリ秒） | 末尾まで |
| `--speed <factor>` | `-s` | 再生速度倍率 | `1.0` |
| `--dry-run` | `-n` | 実際には送信しない | off |
| `--arm-delay <ms>` | | Arm前の待機時間 | `3000` |

**使用例:**
```bash
# 基本的な再生
sudo expresslrs_sender play -H data/flight.csv

# ループ再生、2倍速
sudo expresslrs_sender play -H data/flight.csv --loop --speed 2.0

# ドライランでタイミング確認
expresslrs_sender play -H data/flight.csv --dry-run -v
```

#### `validate` - 操作履歴の検証

操作履歴ファイルの形式と値の妥当性をチェックする。

```bash
expresslrs_sender validate --history <file>
```

| オプション | 短縮 | 説明 |
|-----------|------|------|
| `--history <file>` | `-H` | 検証する操作履歴ファイル |
| `--strict` | | 厳格モード（警告もエラー扱い） |

**出力例:**
```
Validating: data/flight.csv
  Format: CSV
  Frames: 3000
  Duration: 60.0s
  Channels: 8
  Rate: 50Hz (consistent)
  Value range: OK (172-1811)
  Timestamp: OK (monotonic increasing)
Result: VALID
```

#### `ping` - TX モジュールとの接続確認

CRSF デバイスピングを送信し、TX モジュールとの接続を確認する。

```bash
sudo expresslrs_sender ping [オプション]
```

| オプション | 短縮 | 説明 | デフォルト |
|-----------|------|------|-----------|
| `--timeout <ms>` | `-t` | タイムアウト | `1000` |
| `--count <n>` | | ピング回数 | `3` |

**出力例:**
```
Pinging ELRS TX on /dev/ttyAMA0...
Response from BETAFPV NANO TX: time=2.3ms
Response from BETAFPV NANO TX: time=2.1ms
Response from BETAFPV NANO TX: time=2.2ms
--- ping statistics ---
3 packets transmitted, 3 received, 0% packet loss
rtt min/avg/max = 2.1/2.2/2.3 ms
```

#### `info` - デバイス情報の表示

接続されている TX モジュールの情報を表示する。

```bash
sudo expresslrs_sender info
```

**出力例:**
```
Device: BETAFPV NANO TX V2
Firmware: ExpressLRS 3.4.0
RF Protocol: CRSF
Frequency: 2.4GHz
Max Power: 100mW
Packet Rate: 500Hz
```

#### `send` - 単発コマンド送信

指定したチャンネル値を一度だけ送信する（テスト用）。

```bash
sudo expresslrs_sender send [オプション]
```

| オプション | 短縮 | 説明 | デフォルト |
|-----------|------|------|-----------|
| `--channels <values>` | | カンマ区切りのチャンネル値 | 全て中央値 |
| `--duration <ms>` | | 送信継続時間 | `1000` |
| `--arm` | | Armスイッチを有効化 | off |

**使用例:**
```bash
# 全チャンネル中央値で1秒間送信
sudo expresslrs_sender send

# Throttle 50%で送信（Arm有効）
sudo expresslrs_sender send --channels 992,992,992,992 --arm --duration 5000
```

### 終了コード

| コード | 意味 |
|-------|------|
| 0 | 正常終了 |
| 1 | 一般エラー |
| 2 | コマンドライン引数エラー |
| 3 | 設定ファイルエラー |
| 4 | 操作履歴ファイルエラー |
| 5 | UART/デバイスエラー |
| 6 | 安全機能による停止 |
| 130 | SIGINT (Ctrl+C) による終了 |
| 143 | SIGTERM による終了 |

### 設定ファイル形式

`config/default.json`:
```json
{
  "device": {
    "port": "/dev/ttyAMA0",
    "baudrate": 420000
  },
  "playback": {
    "default_rate_hz": 50,
    "arm_delay_ms": 3000
  },
  "safety": {
    "throttle_min": 172,
    "arm_channel": 5,
    "failsafe_timeout_ms": 500
  },
  "logging": {
    "level": "info",
    "file": "/var/log/expresslrs_sender.log"
  }
}
```

## テスト仕様

### テストフレームワーク

- **Google Test** (gtest) を使用
- カバレッジ目標: 80% 以上

### テストファイル構成

```
tests/
├── test_main.cpp           # テストエントリポイント
├── test_crsf.cpp           # CRSF プロトコルテスト
├── test_crc8.cpp           # CRC 計算テスト
├── test_history_loader.cpp # 操作履歴ローダーテスト
├── test_playback.cpp       # 再生コントローラーテスト
├── test_safety.cpp         # 安全機能テスト
├── test_config.cpp         # 設定管理テスト
├── test_cli.cpp            # CLI引数パーステスト
├── integration/
│   └── test_e2e.cpp        # 統合テスト（ドライラン）
└── fixtures/
    ├── valid.csv           # 正常なCSVファイル
    ├── valid.json          # 正常なJSONファイル
    ├── invalid_format.csv  # 不正フォーマット
    ├── out_of_range.csv    # 範囲外の値
    └── config.json         # テスト用設定
```

---

### 1. CRSF プロトコルテスト (`test_crsf.cpp`)

#### 1.1 CRC-8 計算

| ID | テスト項目 | 入力 | 期待値 |
|----|-----------|------|--------|
| CRC-001 | 空データ | `[]` | `0x00` |
| CRC-002 | 単一バイト | `[0x00]` | `0x00` |
| CRC-003 | 単一バイト | `[0xFF]` | `0xD5` |
| CRC-004 | 複数バイト | `[0x16, 0x00, ...]` (RC frame) | 計算値と一致 |
| CRC-005 | 既知のCRSFフレーム | ExpressLRS実機データ | 一致 |

#### 1.2 フレーム構築

| ID | テスト項目 | 条件 | 期待結果 |
|----|-----------|------|----------|
| FRM-001 | RCチャンネルフレーム生成 | 16ch全て中央値 | 正しいバイト列 |
| FRM-002 | RCチャンネルフレーム生成 | 最小値/最大値混在 | 正しいバイト列 |
| FRM-003 | デバイスピングフレーム | - | `[0xC8, 0x02, 0x28, CRC]` |
| FRM-004 | フレーム長 | RCチャンネル | 26バイト |
| FRM-005 | Syncバイト | 全フレーム | 先頭が `0xC8` |

#### 1.3 チャンネル値変換

| ID | テスト項目 | 入力 | 期待値 |
|----|-----------|------|--------|
| CH-001 | PWM→CRSF 最小値 | 988 us | 172 |
| CH-002 | PWM→CRSF 中央値 | 1500 us | 992 |
| CH-003 | PWM→CRSF 最大値 | 2012 us | 1811 |
| CH-004 | CRSF→PWM 逆変換 | 992 | 1500 us |
| CH-005 | 範囲外クランプ | 800 us | 172 (下限) |
| CH-006 | 範囲外クランプ | 2200 us | 1811 (上限) |

#### 1.4 11ビットパッキング

| ID | テスト項目 | 条件 | 期待結果 |
|----|-----------|------|----------|
| PCK-001 | 16ch → 22バイト | 全て0 | 正しくパック |
| PCK-002 | 16ch → 22バイト | 全て最大値 | 正しくパック |
| PCK-003 | アンパック | パック済みデータ | 元の値に復元 |
| PCK-004 | ラウンドトリップ | 任意値 | 一致 |

---

### 2. 操作履歴ローダーテスト (`test_history_loader.cpp`)

#### 2.1 CSV パース

| ID | テスト項目 | 入力 | 期待結果 |
|----|-----------|------|----------|
| CSV-001 | 正常なCSV | `fixtures/valid.csv` | 正常読み込み |
| CSV-002 | ヘッダー行スキップ | ヘッダー付きCSV | データのみ取得 |
| CSV-003 | 8チャンネル | 8列データ | 正常読み込み |
| CSV-004 | 16チャンネル | 16列データ | 正常読み込み |
| CSV-005 | 空ファイル | 0行 | エラー返却 |
| CSV-006 | 不正フォーマット | 列数不一致 | エラー返却 |
| CSV-007 | 数値以外 | 文字列含む | エラー返却 |

#### 2.2 JSON パース

| ID | テスト項目 | 入力 | 期待結果 |
|----|-----------|------|----------|
| JSON-001 | 正常なJSON | `fixtures/valid.json` | 正常読み込み |
| JSON-002 | メタデータ取得 | name, duration | 正しく取得 |
| JSON-003 | フレーム配列 | frames[] | 全フレーム読み込み |
| JSON-004 | 不正JSON | 構文エラー | エラー返却 |
| JSON-005 | 必須フィールド欠落 | framesなし | エラー返却 |

#### 2.3 バリデーション

| ID | テスト項目 | 入力 | 期待結果 |
|----|-----------|------|----------|
| VAL-001 | 値範囲チェック | 0〜2000 | 172未満でエラー |
| VAL-002 | 値範囲チェック | 172〜1811 | 正常 |
| VAL-003 | タイムスタンプ順序 | 昇順 | 正常 |
| VAL-004 | タイムスタンプ順序 | 降順含む | エラー |
| VAL-005 | タイムスタンプ重複 | 同一時刻 | 警告 |

---

### 3. 再生コントローラーテスト (`test_playback.cpp`)

#### 3.1 タイミング制御

| ID | テスト項目 | 条件 | 期待結果 |
|----|-----------|------|----------|
| TIM-001 | 50Hz送信 | 20ms間隔 | ±1ms以内 |
| TIM-002 | 100Hz送信 | 10ms間隔 | ±1ms以内 |
| TIM-003 | 500Hz送信 | 2ms間隔 | ±0.5ms以内 |
| TIM-004 | 2倍速再生 | speed=2.0 | 間隔半分 |
| TIM-005 | 0.5倍速再生 | speed=0.5 | 間隔2倍 |

#### 3.2 再生制御

| ID | テスト項目 | 操作 | 期待結果 |
|----|-----------|------|----------|
| PLY-001 | 開始 | start() | 状態=Playing |
| PLY-002 | 停止 | stop() | 状態=Stopped |
| PLY-003 | 一時停止 | pause() | 状態=Paused |
| PLY-004 | 再開 | resume() | 状態=Playing |
| PLY-005 | ループ再生 | loop=true, 末尾到達 | 先頭に戻る |
| PLY-006 | ループ回数 | loop_count=3 | 3回で停止 |
| PLY-007 | 開始位置指定 | start_time=5000 | 5秒目から開始 |
| PLY-008 | 終了位置指定 | end_time=10000 | 10秒で停止 |

#### 3.3 フレーム取得

| ID | テスト項目 | 条件 | 期待結果 |
|----|-----------|------|----------|
| GET-001 | 現在フレーム取得 | 再生中 | 正しいフレーム |
| GET-002 | 補間なし | 正確な時刻 | そのフレーム |
| GET-003 | 末尾到達 | ループなし | 最後のフレーム維持 |

---

### 4. 安全機能テスト (`test_safety.cpp`)

#### 4.1 Arm インターロック

| ID | テスト項目 | 条件 | 期待結果 |
|----|-----------|------|----------|
| ARM-001 | Disarm状態 | arm_switch=OFF | Throttle=最小値に強制 |
| ARM-002 | Arm状態 | arm_switch=ON | Throttle=指定値 |
| ARM-003 | Arm遷移 | OFF→ON | arm_delay後に有効 |
| ARM-004 | 緊急Disarm | 異常検出時 | 即座にDisarm |

#### 4.2 Failsafe

| ID | テスト項目 | 条件 | 期待結果 |
|----|-----------|------|----------|
| FS-001 | タイムアウト | 500ms無送信 | Failsafe値送信 |
| FS-002 | Failsafe値 | - | Throttle=172, Disarm |
| FS-003 | 復帰 | 正常再開 | 通常動作に戻る |

#### 4.3 シグナルハンドリング

| ID | テスト項目 | シグナル | 期待結果 |
|----|-----------|----------|----------|
| SIG-001 | SIGINT | Ctrl+C | Disarm→終了(130) |
| SIG-002 | SIGTERM | kill | Disarm→終了(143) |
| SIG-003 | 終了処理 | 任意シグナル | 最低10フレームDisarm送信 |

---

### 5. 設定管理テスト (`test_config.cpp`)

| ID | テスト項目 | 条件 | 期待結果 |
|----|-----------|------|----------|
| CFG-001 | 正常読み込み | valid config.json | 全値取得 |
| CFG-002 | デフォルト値 | フィールド欠落 | デフォルト適用 |
| CFG-003 | 不正JSON | 構文エラー | エラー返却 |
| CFG-004 | ファイル不在 | 存在しないパス | エラー返却 |
| CFG-005 | 型不一致 | baudrate="abc" | エラー返却 |

---

### 6. CLI テスト (`test_cli.cpp`)

#### 6.1 引数パース

| ID | テスト項目 | 入力 | 期待結果 |
|----|-----------|------|----------|
| CLI-001 | ヘルプ | `--help` | ヘルプ表示、終了0 |
| CLI-002 | バージョン | `--version` | バージョン表示、終了0 |
| CLI-003 | playコマンド | `play -H file.csv` | 正常パース |
| CLI-004 | 短縮オプション | `-v -d /dev/tty` | 正常パース |
| CLI-005 | 不明オプション | `--unknown` | エラー、終了2 |
| CLI-006 | 必須オプション欠落 | `play` (history無し) | エラー、終了2 |

#### 6.2 サブコマンド

| ID | テスト項目 | 入力 | 期待結果 |
|----|-----------|------|----------|
| SUB-001 | play | `play -H f.csv` | PlayCommand実行 |
| SUB-002 | validate | `validate -H f.csv` | ValidateCommand実行 |
| SUB-003 | ping | `ping` | PingCommand実行 |
| SUB-004 | info | `info` | InfoCommand実行 |
| SUB-005 | send | `send --arm` | SendCommand実行 |
| SUB-006 | 不明コマンド | `unknown` | エラー、終了2 |

---

### 7. 統合テスト (`test_e2e.cpp`)

| ID | テスト項目 | 条件 | 期待結果 |
|----|-----------|------|----------|
| E2E-001 | ドライラン再生 | `play --dry-run` | 正常完了、送信なし |
| E2E-002 | バリデート正常 | `validate` valid file | 終了0 |
| E2E-003 | バリデート異常 | `validate` invalid file | 終了4 |
| E2E-004 | 設定ファイル指定 | `-c custom.json` | 設定反映 |
| E2E-005 | ログ出力 | `-v` | 詳細ログ出力 |

---

### テスト実行方法

```bash
# 全テスト実行
cd build && ctest --output-on-failure

# 特定テスト実行
./test_expresslrs_sender --gtest_filter="CrsfTest.*"

# カバレッジ付き
cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_COVERAGE=ON ..
make && ctest
lcov --capture --directory . --output-file coverage.info
genhtml coverage.info --output-directory coverage_report
```

## 今後の拡張予定

- [ ] リアルタイム操作履歴記録機能
- [ ] WebSocket経由でのリモート制御
- [ ] テレメトリデータの受信・記録
- [ ] 複数機体への同時送信（マルチTX）
