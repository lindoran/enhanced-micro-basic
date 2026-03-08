# MICRO-BASIC 2.1 — TODO

Items are grouped by complexity and listed in recommended implementation order.
Each item references the tag used in source comments for easy searching.

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
