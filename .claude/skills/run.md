# /run - アプリケーションの実行

expresslrs_sender を実行します。

## 前提条件

- プロジェクトがビルド済みであること
- 設定ファイルが存在すること
- （実機テスト時）UART デバイスが接続されていること

## 実行コマンド

```bash
cd /Users/akihito/works/expresslrs_sender
sudo ./build/expresslrs_sender --config config/default.json --history data/sample.csv
```

## オプション

- `/run dry` - ドライラン（実際には送信しない）
- `/run debug` - デバッグモードで実行（詳細ログ出力）
- `/run loop` - 操作履歴をループ再生

## 主要な引数

| 引数 | 説明 | 例 |
|-----|------|-----|
| `--config` | 設定ファイルパス | `config/default.json` |
| `--history` | 操作履歴ファイル | `data/flight.csv` |
| `--device` | UART デバイス | `/dev/ttyAMA0` |
| `--rate` | パケットレート (Hz) | `50` |

## 停止方法

- `Ctrl+C` で安全に停止（Disarm 状態で終了）
