# MICRO-BASIC 2.2 — TODO

Items are grouped by complexity and listed in recommended implementation order.
Each item references the tag used in source comments for easy searching.

## Ready to Implement

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

## Closed — Won't Fix in 2.x

### `TODO(peek)` / `TODO(poke)` — Memory access

**Closed.** PEEK/POKE made sense in the 8-bit era where the memory map was
the hardware. On DOS the use case is thin and on Linux/Windows meaningless.
On embedded targets the right approach is to write device-specific C, not
route hardware access through a BASIC interpreter.

INP/OUT remains — 8-bit port I/O is real and meaningful on DOS and will map
naturally to GPIO/peripheral buses on embedded targets.

PEEK/POKE will not be implemented in 2.x. Revisit only if a specific embedded
port has a clear use case for it.

---

## Deferred to 3.0 — Embedded / Arduino Port

### `TODO(fre)` — `FRE()` function and memory reporting

**Deferred.** `FRE()` requires a meaningful free heap figure. On Linux/Windows
this is pointless. On ia16 DOS the newlib heap accounting does not reflect
conventional memory correctly. On bare metal targets (AVR, Z80) malloc is a
simple bump allocator into known RAM and `FRE()` would work correctly and be
genuinely useful.

Implement in 3.0 when there is an actual bare metal target to test against.
At that point the workspace model may also change — a fixed allocation at
startup rather than dynamic malloc throughout — which makes `FRE()` trivial
pointer arithmetic rather than a heap query.

### 3.0 General Notes

3.0 targets a specific small device (ATmega2560 or similar). It is not a
bolt-on to the current codebase — it is a fresh look at what the interpreter
needs to be on that hardware:

- What fits in flash
- What the RAM budget is
- Whether the linked list program storage makes sense or a flat buffer is better
- Whether the full variable set (260 numeric, 260 string, 260 array) is realistic
- What the right workspace model is (fixed allocation vs dynamic malloc)

2.x is feature-stable. 3.0 starts from the hardware and works backward.

---

## Notes — Windows HAL

The MinGW `<conio.h>` header exposes `inp()` and `outp()`. These are not used
in the Windows HAL. The current explicit no-ops are correct and intentional:

```c
/* inp()/outp() from <conio.h> exist on MinGW but are blocked on all
 * NT-based Windows (XP onward). Port I/O requires ring 0 / a kernel
 * driver. The conio versions are a fossil from the Win9x / Win32s era
 * when the DOS real-mode layer was still present and port access could
 * slip through. We no-op explicitly rather than calling conio to make
 * intent clear and avoid silent failure on Win10/Win11. */
static ubint do_in(ubint p)            { (void)p; return 0; }
static void  do_out(ubint p, ubint v)  { (void)p; (void)v; }
```

This comment is already present in BASIC.c.
