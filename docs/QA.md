# Validation — 2026-11-06

Baseline commit: 3e7e4f4e20e94160b6865e731d0f5cdd9ac793df.

Audited original index.html (1080 lines), scripts, both workflows, repository history and real Actions logs.
- https://github.com/Marolk/Dama/actions/runs/42197095515 : missing build-web.sh.
- https://github.com/Marolk/Dama/actions/runs/42197383751 : missing src/engine.cpp.
- Original web workflow uploaded an artifact but did not deploy Pages.

Local results:
- Native C++20 rules, promotion-at-end, captured blockers, mandatory maximum capture, no moves, forward-only capture, repetition and budget: PASS.
- AddressSanitizer / UndefinedBehaviorSanitizer: PASS with leak detection disabled because this execution environment uses ptrace. Leak checking: NOT VERIFIED.
- 404 native/WASM protocol comparisons: PASS.
- 400 deterministic randomized comparisons against original UI legal generator: PASS (both board sizes and square parities).
- Native 100 ms search: 100.464 ms, depth 7, 74,551 nodes (single initial 10×10 sample).
- WASM 150 ms search: 151.284 ms / depth 8 / 63,329 nodes (8×8), 150.189 ms / depth 6 / 49,006 nodes (10×10). Samples, not latency guarantees.
- Local Chromium and Firefox, 390×844 touch emulation: PASS; actual WASM Worker load, human move, AI legal response, presets, rotation, editing, new game and stale-response cancellation.
- WebKit runs in CI; local execution lacks system libraries.

Physical Android / iPhone / Windows / Mac devices: NOT VERIFIED. Touch emulation and Linux browser engines are not equivalent to physical device testing.

GitHub Actions / deployment and live URL must be checked after the commit; consult the workflow run for that revision. This report does not pre-claim CI or deployment success.
