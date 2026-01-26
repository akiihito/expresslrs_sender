# /check-crsf - CRSF パケットの検証

CRSF プロトコルのパケット構造や CRC 計算を検証します。

## 用途

- 新しい CRSF フレームタイプの実装時
- パケット構造のデバッグ
- CRC 計算の確認

## CRSF フレーム構造

```
[0xC8] [Length] [Type] [Payload...] [CRC8]
```

## 主要なフレームタイプ

| Type | 値 | Payload サイズ |
|------|-----|---------------|
| RC_CHANNELS_PACKED | 0x16 | 22 bytes |
| LINK_STATISTICS | 0x14 | 10 bytes |
| DEVICE_PING | 0x28 | 0 bytes |
| DEVICE_INFO | 0x29 | 可変 |

## CRC-8 DVB-S2 計算

多項式: `0xD5`

```cpp
uint8_t crc8_dvb_s2(uint8_t crc, uint8_t data) {
    crc ^= data;
    for (int i = 0; i < 8; i++) {
        crc = (crc & 0x80) ? (crc << 1) ^ 0xD5 : crc << 1;
    }
    return crc;
}
```

## RC チャンネル値の確認

| 状態 | CRSF 値 | PWM (us) |
|-----|--------|----------|
| 最小 | 172 | 988 |
| 中央 | 992 | 1500 |
| 最大 | 1811 | 2012 |

## パケット例

### RC Channels (16ch, all center)
```
C8 18 16 [22 bytes channel data] [CRC]
```

### Device Ping
```
C8 02 28 [CRC]
```

## デバッグコマンド

操作履歴ファイルの検証:
```bash
./build/expresslrs_sender --validate data/sample.csv
```

パケットダンプ:
```bash
./build/expresslrs_sender --dump-packets --history data/sample.csv
```
