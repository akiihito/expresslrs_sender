# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/).

## [Unreleased]

### Added
- RT スケジューリングユーティリティ (`src/scheduling/realtime.hpp/.cpp`)
  - `SCHED_FIFO` + `mlockall` でリアルタイム優先度設定
  - root 権限がない場合は警告を出して通常動作を継続
  - `#ifdef __linux__` ガードで macOS ビルドも対応
- `--no-realtime` CLI オプション（RT スケジューリングを無効化）
- 設定ファイル `scheduling.realtime` セクション
- `PlaybackStats::max_jitter_us` フィールド（最大ジッター追跡）
- タイミングテスト (`tests/test_timing.cpp`)

### Changed
- 500Hz タイミング精度を改善
  - PlaybackController の `tick()` にドリフト補正を導入（`m_last_send_time += interval`）
  - 3 インターバル以上遅れた場合はスナップフォワードでバースト送信を防止
  - `cmdSend` にも同様のドリフト補正を適用
  - `send_interval` を `milliseconds(2)` から `microseconds(2000)` に変更（精度向上）
- メインループのスリープ戦略を改善
  - 固定 `sleep_for(100µs)` から次回送信時刻までの残り時間ベースに変更
  - 残り > 200µs の場合は `sleep_for(remaining - 200µs)`、それ以外はスピンウェイト
- 再生完了ログに `max_jitter` を追加表示

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
- 半二重テレメトリドレイン (`UartDriver::drainTelemetry()`)
  - RC フレーム送信後に TX モジュールからのテレメトリ応答を読み捨て
  - 半二重 S.Port 上でのバス衝突を防止し、ch5-ch16 が正しく送信されるよう修正
  - `cmdPlay` / `cmdSend` の送信ループおよび disarm ループに適用
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
