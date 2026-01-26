# ExpressLRS Sender - Claude Code ガイドライン

## プロジェクト概要

Raspberry Pi + BETAFPV NANO TX V2 を使用した ExpressLRS 操作履歴再生システム。
C++17 で実装し、CRSF プロトコルで TX モジュールと通信する。

## 重要なファイル

- `SPECIFICATION.md` - 詳細な仕様設計書（必ず参照すること）
- `src/crsf/crsf.cpp` - CRSF プロトコル実装（コア機能）
- `src/playback/playback_controller.cpp` - 再生制御ロジック

## コーディング規約

### C++ スタイル
- C++17 標準を使用
- Google C++ Style Guide に準拠
- ヘッダーガードは `#pragma once` を使用
- 名前空間: `elrs` をルート名前空間として使用

### 命名規則
- クラス名: PascalCase (`CrsfProtocol`, `PlaybackController`)
- 関数名: camelCase (`sendFrame`, `loadHistory`)
- 変数名: snake_case (`packet_rate`, `channel_data`)
- 定数: UPPER_SNAKE_CASE (`MAX_CHANNELS`, `CRSF_SYNC_BYTE`)
- メンバ変数: `m_` プレフィックス (`m_uart`, `m_config`)

### エラーハンドリング
- 例外は使用しない（組み込み系との互換性のため）
- `std::optional` または `std::expected` (C++23) を使用
- エラーログは spdlog で出力

### メモリ管理
- スマートポインタを優先 (`std::unique_ptr`, `std::shared_ptr`)
- 生ポインタは参照のみに限定
- RAII パターンを徹底

## ビルド方法

```bash
mkdir build && cd build
cmake ..
make -j4
```

## テスト実行

```bash
cd build
ctest --output-on-failure
```

## 安全に関する注意

### 絶対に守ること
1. **Arm 状態の管理**: Arm スイッチが明示的に ON にならない限り、Throttle は最小値を維持
2. **Failsafe 実装**: 異常時は必ず安全な値（Throttle 最小、Disarm）を送信
3. **シグナルハンドリング**: SIGINT/SIGTERM で即座に Disarm して終了

### コードレビュー重点項目
- `safety/` ディレクトリ内のコード変更は特に慎重に
- チャンネル値の範囲チェック（172〜1811）
- タイミングクリティカルな処理のレイテンシ

## CRSF プロトコル実装のポイント

### CRC 計算
```cpp
// CRC-8 DVB-S2 多項式: 0xD5
uint8_t crc8_dvb_s2(uint8_t crc, uint8_t data) {
    crc ^= data;
    for (int i = 0; i < 8; i++) {
        crc = (crc & 0x80) ? (crc << 1) ^ 0xD5 : crc << 1;
    }
    return crc;
}
```

### チャンネル値変換
```cpp
// PWM (us) → CRSF 値
constexpr int16_t pwmToCrsf(int16_t pwm) {
    // 988-2012us → 172-1811
    return (pwm - 988) * (1811 - 172) / (2012 - 988) + 172;
}
```

## よく使うコマンド

| コマンド | 説明 |
|---------|------|
| `cmake -DCMAKE_BUILD_TYPE=Debug ..` | デバッグビルド |
| `cmake -DCMAKE_BUILD_TYPE=Release ..` | リリースビルド |
| `make clean` | ビルドクリーン |
| `./expresslrs_sender --help` | ヘルプ表示 |

## 依存ライブラリのインストール（Raspberry Pi）

```bash
sudo apt update
sudo apt install -y cmake build-essential
sudo apt install -y nlohmann-json3-dev libspdlog-dev
```

## デバッグ Tips

- UART 通信の確認: `minicom -D /dev/ttyAMA0 -b 420000`
- GPIO 状態確認: `gpio readall`
- プロセス優先度確認: `ps -eo pid,ni,pri,comm | grep expresslrs`

## 関連ドキュメント

- [ExpressLRS Documentation](https://www.expresslrs.org/software/switch-config/)
- [CRSF Protocol Specification](https://github.com/ExpressLRS/ExpressLRS/wiki/CRSF-Protocol)
- [BETAFPV NANO TX Module](https://betafpv.com/products/elrs-nano-tx-module)
