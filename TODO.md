# MICRO-BASIC 2.1 — TODO

Items are grouped by complexity and listed in recommended implementation order.
Each item references the tag used in source comments for easy searching.


### `TODO(bitshift)` — Bit shift operators `<<` and `>>`

Two new operator tokens at priority 3 (same level as `&` `|` `^`).

```
SHL  56   <<   (ubint)op1 << op2
SHR  57   >>   (ubint)op1 >> op2
```

`ubint` cast required on both — logical shift not arithmetic. Consistent with
existing bitwise operator behaviour.

**Important:** `"<<"` and `">>"` must appear **before** `"<"` and `">"` in
`reserved_words[]` so the longer match wins. Same pattern as `<=` and `>=`
already use.

Changes required:
- Two new `#define` tokens
- Two entries in `reserved_words[]`
- Two entries in `priority[]`
- Two cases in `do_arith()`

Approximately 15 lines of new code total. Implement after `TODO(literals)` is
stable and tested.

---

### `TODO(unsigned-compare)` — `UGT()` and `ULT()` unsigned comparison functions

All comparison operators (`<` `<=` `>` `>=`) are signed — same contract as 6809
`BGT`/`BLT`. For unsigned comparison the XOR trick works in the meantime:

```basic
IF (A0 ^ #8000) > (B0 ^ #8000) THEN ...  : REM unsigned >
```

`UGT(a,b)` and `ULT(a,b)` are cleaner helpers returning 1/0 like the comparison
operators:

```basic
IF UGT(A0, #8000) THEN ...
IF ULT(A0, B0) THEN ...
```

Implementation: cast both operands to `ubint` before comparing. Two new tokens,
two cases in `get_value()`. No other changes needed. Low priority — the XOR trick
works fine until this is implemented.

---

## Medium Complexity — Implement Second

### `TODO(reljmp)` — Forward relative jumps

Forward-only relative jumps for `GOTO`, `GOSUB`, and `IF`. No back pointer
needed — singly linked list stays as-is. Backwards jumps use absolute line
numbers as before.

```basic
GOTO +4              : REM skip 4 lines forward
GOSUB +10            : REM call subroutine 10 lines ahead, return here
IF X >= 10 THEN +3   : REM skip 3 lines if true
```

`-` prefix is a hard error — catches accidental signed literals.

**Three places to update together:**
1. `GOTO` / `GOSUB` handler in `execute()`
2. `IF` handler in `execute()`
3. LIF gets relative jumps **for free** via `GOTO` in the body — no changes needed

Implementation sketch (same pattern in both GOTO and IF):
```c
c = skip_blank();
if (c == '+') {
    ubint offset = get_num();
    lp = runptr;
    while (offset-- > 0 && lp) lp = lp->Llink;
    if (!lp) error(3);
    return lp; }
if (c == '-') error(0);   /* relative back not supported */
cmdptr--;                  /* put char back for eval_num() */
return find_line((ubint)eval_num());
```

Use cases:
- While-loop pattern: `IF X >= 10 THEN +3` skips loop body
- Skip-ahead: `IF F$ = "" THEN +4` skips file open block
- Tight subroutines without inventing line numbers: `GOSUB +5`

Verify carefully: GOSUB relative push/pop, edge case of jumping past end of
program, interaction with control stack.

---

## High Complexity — Design First, Implement Later

### `TODO(peek)` / `TODO(poke)` — Memory access

`PEEK(addr)` numeric function and `POKE addr, val` statement, alongside existing
`INP`/`OUT`.

```basic
A0 = PEEK(#B800)     : REM read byte at address
POKE #B800, #41      : REM write byte to address
```

HAL isolation is critical — must be airtight across all targets:
- **ia16 DOS**: direct dereference, works in real mode. Bad address crashes machine.
- **Linux**: `/dev/mem` opens security issues. Likely no-op or `error(0)`.
- **Windows**: no-op or `error(0)`.
- **AVR/6809**: direct dereference, meaningful for hardware registers.

RPEEK/RPOKE register alias table to follow once PEEK/POKE are stable. The alias
table maps register names (e.g. `PORTB`) to addresses in a platform-specific HAL
block — BASIC program stays readable across targets, only the table changes.

Implement after prefix literals and bit shifts are done and tested.

---

### `TODO(jmp-cache)` — Jump cache for `find_line()`

Needs more design discussion before any code is written.

**Problem:** `find_line()` does a linear scan of the program list on every
`GOTO`/`GOSUB`. On large programs this is slow, especially in tight loops.

**Rough idea:** flat cache of `{ ubint lno, struct line_rec *ptr }` pairs.

Two population strategies under consideration:

**Lazy (runtime):** on cache miss do linear scan as normal, store result. Same
target pays scan cost once only. Simple, works for interactive use.

**Eager (load/run time):** single pass over all `Ltext` at `LOAD` or `RUN`,
find every `GOTO`/`GOSUB` token, read line number, pre-populate. Cache fully
warm before first instruction. Also catches undefined line number errors early.
Since MICRO-BASIC has no computed GOTOs, static scan covers 100% of targets.

**Combined:** eager seed at load/run time, lazy fill as fallback.

Open questions to resolve before implementing:
- Eager vs lazy vs combined — which fits the codebase best?
- Eviction policy when cache is full (oldest? stop caching? error?)
- Pointer width: `line_rec*` is 2 bytes on ia16, 4/8 on larger targets — cache struct must work on all platforms
- Invalidation: clear on `NEW`, `LOAD`, or any line edit

Tuning define:
```c
#ifdef SMALL_TARGET
#  define JMP_CACHE_SIZE  16
#else
#  define JMP_CACHE_SIZE  128
#endif
```

---

## Outside the Source — Discussed, Not Yet TODO'd

### ALSA beep HAL for Linux
Current Linux HAL sends a terminal bell — not useful. ALSA tone generator makes
`BEEP freq, ms` work correctly on Linux. Code exists in a separate conversation.
Slots into the HAL `#ifdef` block, no other changes needed. Bring over when ready.

### Fixed point 8.8
Decided this is a **programming technique, not a language feature**. Once bit
shifts are implemented, 8.8 fixed point arithmetic is fully expressible in BASIC:

```basic
REM 8.8 multiply: zr_new = ((zr*zr - zi*zi) >> 8) + cr
```

No interpreter changes needed. Manual to include a section explaining the
convention with Mandelbrot as a worked example.

### RPEEK / RPOKE — Register alias table
Revisit when actively targeting AVR or 6809. Register names map to addresses in
a platform-specific HAL table. Depends on PEEK/POKE being stable first.
