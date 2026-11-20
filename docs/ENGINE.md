# Dama C++20 / WASM

The C++ source is authoritative for both legal UI moves and AI search. JavaScript only handles rendering, animation, worker messages and game-history hashes. No JavaScript AI fallback exists. The old UI generator is retained only in tests/reference-rules.js, not shipped.

## Rules
Original presets: 8×8 / 12 pieces or 10×10 / 15 pieces, three starting rows, either dark-square parity. Positive player moves upward; negative player downward. Men move and capture forward only. Kings fly in all four diagonal directions. Captures are compulsory and globally maximum by number. Captured pieces block until sequence completion; promotion occurs only at turn end. Removed original artificial twelve-capture cap. Threefold repetition (board plus side) is a draw, including within search.

## Protocol v1
Worker input: `{id, op:0|1, board:number[][], player:1|-1, difficulty:1..5, history:string[]}`. `op=0` generates legal moves; `op=1` searches. History excludes current position and contains decimal 64-bit hashes as strings (never JS numbers).

C ABI: `const char* dama_request(const char*)`; response buffer valid until next call. Whitespace-separated request: `1 op size side milliseconds maxDepth cells... historyCount hashes...`. C++ clamps budget to 1–2800 ms and depth to 1–30. Response JSON includes version, hash and all legal moves; search adds best, draw, completed depth, nodes, elapsedMs, score, proof and up to four ranked plans. Coordinates are zero-based; path is row-major landing indices. Captures and steps retain complete sequence order. Errors return an explicit error field.

## Search and limits
Iterative deepening, full-width PVS/alpha-beta, forced-capture quiescence, deterministic 64-bit Zobrist-style keys, bounded transposition table, killer and history ordering, material/advancement/centralization evaluation, path repetition. TT is deliberately used for ordering only: scores are history-dependent under repetition and must not be reused as context-free cutoffs. No null-move pruning or speculative late-move reductions.

All root alternatives are scored before publishing a completed iteration. A timeout keeps the last complete iteration (or the first legal move when none completes). Worker watchdog terminates a stalled search and shows an error; new game/editor terminates the worker and invalidates queued animations. Root legal generation and initialization precede timed search; the watchdog is the outer protection. Pathological edited positions and slow devices may exceed the nominal budget. No hard three-second hardware guarantee.

Plans A–D are depth-limited evaluations, not proofs. Exchanges, sacrifices and counterattacks are explored through legal forced captures. The sacrifice regression exhaustively checks immediate forced replies in its fixture; it does not prove game-theoretic victory. There is no general exhaustive trap-proof detector or trained strategic planner. Scores are centipawn-like units (man 100, king 330), from side-to-move perspective.

## Reproduce
Emscripten 4.0.23; C++20 compiler; Node 22+; Playwright 1.56.1.

```
mkdir -p build
g++ -std=c++20 -O2 tests/engine_test.cpp -o build/engine_test
./build/engine_test
g++ -std=c++20 -O2 -DDAMA_CLI src/engine.cpp -o build/engine-cli
bash build-web.sh
node tests/protocol.cjs
npm install --no-save playwright@1.56.1
npx playwright install --with-deps chromium firefox webkit
node tests/browser.cjs
```

The site artifact is dist/. Serve via HTTP(S), not file://. The original Android Studio ZIP remains untouched; this browser release does not rebuild that separate native wrapper.
