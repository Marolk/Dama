#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
mkdir -p dist
em++ src/engine.cpp -std=c++20 -O3 --no-entry \
  -s MODULARIZE=1 -s EXPORT_NAME=createDamaEngine \
  -s STACK_SIZE=2097152 -s ALLOW_MEMORY_GROWTH=1 -s ENVIRONMENT=web,worker,node \
  -s DISABLE_EXCEPTION_CATCHING=0 \
  -s EXPORTED_FUNCTIONS='["_dama_request"]' \
  -s EXPORTED_RUNTIME_METHODS='["ccall"]' -o dist/engine.js
cp index.html dama-worker.js dist/
touch dist/.nojekyll
for asset in engine.js engine.wasm dama-worker.js index.html; do test -s "dist/$asset"; done
