# /test - テストの実行

ユニットテストを実行します。

## 前提条件

- プロジェクトがビルド済みであること（`/build` を先に実行）

## 手順

1. build ディレクトリに移動
2. ctest でテストを実行
3. 結果を報告

## 実行コマンド

```bash
cd /Users/akihito/works/expresslrs_sender/build
ctest --output-on-failure
```

## オプション

- `/test verbose` - 詳細出力（`ctest -V`）
- `/test crsf` - CRSF 関連テストのみ実行
- `/test coverage` - カバレッジ付きでテスト実行

## 失敗時の対応

テストが失敗した場合:
1. 失敗したテスト名を特定
2. テストコードを確認（`tests/` ディレクトリ）
3. 対応するソースコードを修正
4. 再テスト
