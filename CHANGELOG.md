# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/).

## [Unreleased]

### Changed
- ソフトウェア信号反転 (`invert_tx`/`invert_rx`) を削除し、dtoverlay 方式に変更
  - `UartOptions`、`AppConfig` から `invert_tx`/`invert_rx` フィールドを削除
  - `invertByte()` 関数を削除
  - 信号反転は Raspberry Pi の dtoverlay (`txdN_invert`) で設定する方式に統一
- TX モジュール通信パラメータを ELRS V3.x 仕様に修正
  - デフォルトボーレートを 420000 → 921600 に変更（TX モジュール用）
  - 旧値は `CRSF_BAUDRATE_RX` として保持（レシーバー直接接続用）
  - デフォルト `invert_tx` を true に変更
  - デフォルトパケットレートを 50Hz → 500Hz に変更

### Added
- 半二重通信サポート (`half_duplex` 設定)
  - `UartOptions` に `half_duplex` フィールド追加
  - 半二重モード時は `tcdrain()` で送信完了を保証
  - `UartDriver::setTxEnabled()` メソッド追加（将来の GPIO 制御用）
- GPIO-UART マッピングユーティリティ (`src/gpio/gpio_uart_map.hpp/.cpp`)
  - GPIO TX ピン番号から UART デバイスパスを自動解決
  - Pi 4/5 の UART0/2/3/4/5 に対応（UART1 mini UART は除外）
- `--gpio <pin>` CLI オプション（GPIO ピン指定で UART デバイスを自動選択）
- `gpio` サブコマンド（利用可能な UART-GPIO マッピングをテーブル表示）
- 設定ファイルの `device.gpio_tx` フィールド
- GPIO-UART マッピングのユニットテスト (`tests/test_gpio_uart_map.cpp`)
- 設定ファイル gpio_tx パースのテスト

### Fixed
- `.gitignore` の `expresslrs_sender` パターンが `include/expresslrs_sender/` ディレクトリにマッチしていた問題を修正
- README 接続表を NANO TX V2 の実際のハードウェア仕様に合わせて修正
  - S.Port ピン1本によるハーフデュプレックス通信（TX/RX 2ピンではない）
  - 電源は 7V〜13V（2S〜3S LiPo）が必要（5V では動作しない）
  - 外部電源を含む接続図を追加（GND 共通）

## [0.1.0] - Initial Release

### Added
- CRSF プロトコル実装（RC チャンネルフレーム送信）
- UART ドライバ（カスタムボーレート対応）
- 操作履歴再生（CSV/JSON 形式）
- 安全機能（Arm インターロック、Failsafe、緊急停止）
- CLI サブコマンド: play, validate, ping, info, send
- JSON 設定ファイル対応
