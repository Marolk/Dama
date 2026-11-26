# REMONTADA — strategy 7

Android version 3.2-remontada, versionCode 5. Difficulty buttons are ordered
2, 3, 4, 5, 6, 7. Easy (1) is removed from selection; its saved settings migrate
to Medium (2) without resetting the board. Other saved difficulty IDs are stable.

## Actual engine behavior

`remontada.h` is C++20, included by the Android JNI engine and the browser's
C++/WASM engine. Strategy 7 is a separate search, not an alias for Impossible.
Existing Android strategies 2–6 and browser strategies 2–5 keep their searches.
Browser strategy 6 now uses the Android Impossible engine as well.

- Full-window root comparisons at equal completed depth; partial iterations
  are discarded on timeout. Existing 0.5/1/2/3-second Android settings remain.
- Negamax/PVS against adversarial replies, capture quiescence without stand-pat
  during mandatory captures, and bounded extensions after offers/promotions.
- Three extension credits per line, four when the starting side has at most
  four pieces. There is no instruction to deliberately reduce its army.
- No null-move, futility or late-move reductions. Transpositions are used only
  for move ordering, so repetition-path-dependent scores are not reused.
- The primary evaluation is the existing tactical material/position evaluation.
  It receives **no sacrifice bonus**. At depth four or more, only equally scored,
  non-negative root candidates can be distinguished by comeback style.
- Style prefers newly offered pieces, then the minimum countercapture across
  **all** forced opponent replies, then preserving one to four pieces, then a
  promotion. These style features never compensate for a lower search score.
- No fixed limit on the number of sacrificed pieces; legal forced captures
  and the adversarial search determine whether an offer survives analysis.
- Time and memory are bounded (131,072 move-order entries, 96-ply limit).
  No server, account or online strength is fabricated. Android Online stays
  disabled because this project has no configured real online engine.

This is a finite search, not a solved-game oracle. Equal evaluations are not
proofs of equal game-theoretic value. It cannot guarantee a comeback, avoid
every eventual loss, or make the human believe anything. A positive score is
an estimate unless the search reaches a forced terminal win. Repetition in
the new search is detected within its search line; the browser also checks
saved game repetition at the root. These are the existing platform limits.

## Verification

`remontada-tests.cpp` checks every opponent reply in the one-for-three fixture,
a depth-six two-piece sacrifice, an independently verified winning alternative
to a poisoned king offer, promotion, wins retaining one to four pieces, legal
moves and deadlines on both board sizes, and protocol strategy 7.

A controlled ablation uses the same position, evaluation and depth four:
without the style preference the engine chooses 46→39; REMONTADA chooses the
offer 46→37. Both score 331. This demonstrates a real decision difference,
without accepting a lower score for the appearance of a sacrifice.

`tests/difficulty.cjs` checks all six selections, exact Arabic order, exclusive
active state, persistence after reload, Easy-save migration and both languages.
`android-ui-undo.test.cjs` also dispatches each visible level to the compiled
native CLI and checks strategy 7, alongside existing Undo/capture/AI tests.
The WASM protocol tests check actual strategies 6 and 7.

APK acceptance checks include native libraries for all four existing ABIs and
exact equality between packaged and tested HTML. A physical Android handset
is still needed to confirm device-specific runtime behavior.
