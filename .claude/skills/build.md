# /build - プロジェクトのビルド

プロジェクトをビルドします。

## 手順

1. `build` ディレクトリが存在しなければ作成
2. CMake を実行して Makefile を生成
3. make でビルドを実行

## 実行コマンド

```bash
cd /Users/akihito/works/expresslrs_sender
mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```

## オプション

- `/build debug` - デバッグビルド（`-DCMAKE_BUILD_TYPE=Debug`）
- `/build clean` - クリーンビルド（build ディレクトリを削除して再ビルド）

## 成功時の確認

- `build/expresslrs_sender` 実行ファイルが生成されていること
- コンパイルエラー・警告がないこと
