#!/usr/bin/env bash
set -euo pipefail

mkdir -p dist

if [ ! -f src/engine.cpp ]; then
  echo 'ERROR: src/engine.cpp is missing. Upload the C++ engine source first.'
  exit 1
fi

em++ src/engine.cpp \
  -std=c++20 -O3 \
  -s WASM=1 \
  -s ALLOW_MEMORY_GROWTH=1 \
  -s ENVIRONMENT=web,worker \
  -o dist/engine.js

cp index.html dist/index.html

echo 'WebAssembly build complete.'
