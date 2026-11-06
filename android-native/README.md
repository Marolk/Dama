# Impossible (level 6): native C++20 tactical search

The Android build overlays engine.cpp from this directory onto the original ZIP project. Levels 1–5 retain their original search. Level 6 selects TrapMaster; it has a persistent transposition table, deterministic Zobrist keys, iterative deepening, PVS, killer/history ordering, capture quiescence, positional evaluation and bounded extensions for forced replies to quiet offers. The AI remains native through the existing JNI bridge.

No target piece count or artificial handicap is imposed. An offered piece must survive adversarial search. Repeated sacrifices can be selected if they remain the best evaluated plan. Neither the name Impossible nor a positive score guarantees victory.

## Protocol

Request: boardSize side timeMs strength followed by boardSize² integer cells.
- boardSize: 8 or 10; side: +1 or -1; cells: 0, ±1 pawn, ±2 king.
- strength 1–5: original worker-count setting; strength 6: dedicated tactical search.
- Search time clamps to 50–2900ms internally; the interface defaults to 3000ms.

Response: from to captureCount captures… pathCount landings… depth score nodes trapCode offeredCount.
Existing UI fields remain in the same order. Extra metadata is appended.
- trapCode 0: no newly forced offer identified.
- trapCode 1: newly forced offer selected with a positive searched score; probable tactic.
- trapCode 2: newly forced offer with a non-positive score; risky or defensive.
These are estimates, never exhaustive proofs of victory.

## Validation

trap-tests.cpp checks a 3-versus-6 fixture, including the mirrored position used by Android's AI. Pawn 17->10 offers one piece; every legal opponent reply captures one piece, then the flying king has a maximum triple capture. Only that short exchange is exhaustively checked. The whole game is not proven won. A separate fixture checks rejection of an immediately losing king sacrifice.

Native budget/legality checks cover 8×8 and 10×10 at 50ms, 250ms and 2900ms. A local x86 run measured 2900ms on both initial boards; this is not a mobile latency guarantee. At 250ms, the initial 10×10 position reached depth 9 versus depth 7 for legacy level 5 in that run; depths and playing strength depend on position and hardware.

The browser test sends real level-6 requests to the compiled C++ engine, plays the forced human capture, checks the triple AI countercapture and restores the position with Undo. GitHub Actions also builds all four Android ABIs and validates the packaged UI and native libraries. Physical Android/JNI runtime testing remains unverified.
