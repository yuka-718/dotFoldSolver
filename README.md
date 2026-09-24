# Dot Fold Solver

8×8 のドット絵から、平坦折り可能な折り紙の展開図を探索するプロジェクトです。

## Web版

`web/` にブラウザ版の UI、`.github/workflows/deploy-pages.yml` に GitHub Pages のビルド・公開設定があります。GitHub Actions 上で C++ の探索エンジンを WebAssembly に変換するため、サーバーへ入力を送らずブラウザ内で計算できます。

ブラウザ版では次の機能を利用できます。

- 8×8 ドット絵のクリック／ドラッグ入力
- 境界線数・回路長のリアルタイム解析
- 平坦折り可能な展開図の探索と描画
- 図形を再現できる共有 URL
- 生成した展開図の SVG 保存

## デスクトップ版

`main.py` を実行すると Tkinter の入力画面が起動します。詳しくは `使い方.txt` を参照してください。

## WebAssembly をローカルでビルド

[Emscripten](https://emscripten.org/) の `emcc` を利用します。

```sh
emcc -std=c++20 -O3 --no-entry \
  dotToGraph.cpp BoundaryGraph.cpp foldsToEdges.cpp loopToFolds.cpp ftcp.cpp \
  -o web/solver.js \
  -s MODULARIZE=1 \
  -s EXPORT_NAME=createDotFoldSolver \
  -s ENVIRONMENT=worker \
  -s ALLOW_MEMORY_GROWTH=1 \
  -s INITIAL_MEMORY=268435456 \
  -s NO_EXIT_RUNTIME=1 \
  -s EXPORTED_FUNCTIONS='["_dotfold_loop_length","_dotfold_solve"]' \
  -s EXPORTED_RUNTIME_METHODS='["ccall"]'
```

生成後はリポジトリのルートで静的 HTTP サーバーを起動し、`web/` を開いてください。

## Original project

Original source: [sakusaku858/dotFoldSolver](https://github.com/sakusaku858/dotFoldSolver)
