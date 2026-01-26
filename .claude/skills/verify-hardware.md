# /verify-hardware - ハードウェア接続の確認

Raspberry Pi と BETAFPV NANO TX V2 の接続状態を確認します。

## 確認項目

1. UART デバイスの存在確認
2. GPIO ピンの状態確認
3. シリアル通信テスト

## 実行コマンド

### 1. UART デバイスの確認
```bash
ls -la /dev/ttyAMA0 /dev/serial0 2>/dev/null || echo "UART device not found"
```

### 2. シリアルポート設定の確認
```bash
stty -F /dev/ttyAMA0 2>/dev/null || echo "Cannot access UART"
```

### 3. シリアルコンソールの無効化確認
```bash
cat /boot/cmdline.txt | grep -v console=serial
```

## トラブルシューティング

### UART が見つからない場合
```bash
# config.txt でUARTを有効化
sudo raspi-config
# Interface Options → Serial Port → No (login shell) → Yes (hardware)

# または直接編集
echo "enable_uart=1" | sudo tee -a /boot/config.txt
sudo reboot
```

### Permission denied の場合
```bash
sudo usermod -a -G dialout $USER
# 再ログインが必要
```

### TX モジュールとの通信テスト
```bash
# CRSF デバイスピング送信（プロジェクトのテストツールを使用）
sudo ./build/expresslrs_sender --ping
```

## 正常時の出力例

```
UART Device: /dev/ttyAMA0
Baudrate: 420000
TX Module: BETAFPV NANO TX V2 detected
Firmware: ExpressLRS 3.x.x
Link Status: OK
```
