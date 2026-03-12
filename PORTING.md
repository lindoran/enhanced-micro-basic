# PORTING NOTES — Embedded / Bare Metal (3.0)

This document tracks what needs to change to port Enhanced Micro-BASIC to a
bare metal target (ATmega2560 or similar). It is a working checklist, not a
design spec. Items will be promoted to proper design decisions when 3.0 work begins.

---

## I/O — stdio must go

The remaining stdio dependency after the 2.3 printf elimination:

| Function | Used for | Replacement |
|---|---|---|
| `fgets` | interactive input, LOAD from file | serial read / line buffer from HAL |
| `fputs` / `putc` | all output (PRINT, errors, banner, LIST) | serial write from HAL |
| `fflush` | flush after PRINT | no-op or HAL flush |
| `fopen` / `fclose` | LOAD, SAVE, OPEN#n | no filesystem on bare metal — stub or remove |
| `FILE *` | filein / fileout / files[] | replace with HAL stream handles |

A serial-only build with no file I/O is the expected first 3.0 configuration.
`LOAD` and `SAVE` could work via serial (XMODEM or plain text paste) but that
is a separate feature decision.

`stdin` / `stdout` / `stderr` references in main() also need replacing.

---

## Memory allocation — malloc must go

Current model: `allocate()` wraps `calloc()`, called for every string assignment,
every program line insert, every DIM array. `free()` called on clear/reassign.

On bare metal there is no heap. Options:

- **Bump allocator** — a fixed block of RAM, pointer advances on alloc, reset on
  NEW/CLEAR. Simple, fast, no fragmentation tracking. `FRE()` becomes trivial.
  Downside: no per-string free — need to accept that string RAM is only reclaimed
  on NEW/CLEAR, not on reassignment.
- **Fixed workspace** — pre-allocate all variable storage at startup from a
  statically sized block. No dynamic allocation at all. Most predictable for
  fixed-RAM targets.

The linked list program storage (one `allocate()` per line) is the biggest
consumer of dynamic allocation. A flat program buffer may be more appropriate
on a target with known fixed RAM.

---

## Variable set sizing

Full set (260 numeric + 260 string + 260 array) costs:
- `num_vars[260]`  — 520 bytes (int16_t)
- `char_vars[260]` — 520 bytes (pointers, 16-bit on AVR)
- `dim_vars[260]`  — 520 bytes (pointers)
- `dim_check[260]` — 520 bytes (ubint)
- Total: ~2080 bytes just for variable tables, before any string or array content

On an ATmega2560 (8K RAM) this is a significant fraction. SMALL_TARGET halves
the set to 130 vars (~1040 bytes). A minimal set (52 vars, A0-Z1) costs ~416 bytes.
Actual choice depends on RAM budget after program storage and stack are accounted for.

---

## Control stack

`ctl_stk[CTL_DEPTH]` where each entry is `bptr` (pointer-width).
On AVR `bptr` should be `uint16_t` (flat 64K address space).
CTL_DEPTH=24 (SMALL_TARGET) costs 48 bytes at 16-bit — acceptable.

---

## RNG

`rand()` / `srand()` not available. Replace with XOR-shift generator.
`time(NULL)` not available for seeding — use a hardware timer tick,
an ADC noise reading, or a fixed seed with a user-settable seed statement.
See `TODO: XOR SHIFT` comment in `get_value()` / `main()`.

---

## String accumulators

`sa1[SA_SIZE]` and `sa2[SA_SIZE]` are static globals — fine on bare metal,
no dynamic allocation. SA_SIZE should be tuned to the target's RAM budget.
Minimum useful value is probably 32-40 bytes for typical embedded string use.
SA_SIZE >= BUFFER_SIZE is enforced at compile time.

---

## Input line buffer

`buffer[BUFFER_SIZE]` — static global, fine. BUFFER_SIZE=80 is comfortable
on a 40-column terminal; could drop to 64 or lower on a constrained target.
This also sets the maximum program line length.

---

## RODATA / flash strings

The `RODATA` / `RD_BYTE` / `RD_PTR` abstraction layer is already in place for
AVR PROGMEM. `reserved_words[]` and `error_messages[]` are declared `const` and
will land in flash automatically with AVR-GCC and `PROGMEM` annotation.
This is already wired — just needs `-DAVR_PROGMEM` at build time.

---

## HAL functions needed

These are already isolated behind the platform HAL block in BASIC.c:

| Function | Current (DOS/Linux) | Bare metal need |
|---|---|---|
| `do_beep(freq, ms)` | PC speaker / ALSA | PWM tone on a timer |
| `do_delay(ms)` | `nanosleep` / BIOS tick | `_delay_ms()` or timer |
| `kbtst()` | termios non-blocking read | UART RX non-blocking |
| `do_in(port)` / `do_out(port, val)` | no-op (Linux/Win) | AVR `_SFR_IO8` or direct port |

---

## Things to call out in the source (not yet marked)

- All `fgets` call sites need a `/* HAL: replace with serial line read */` comment
- All `fputs` / `putc` / `fflush` call sites need `/* HAL: replace with serial write */`
- `fopen` / `fclose` in LOAD/SAVE/OPEN need `/* HAL: no filesystem on bare metal */`
- `system()` in DOS statement — no-op or remove on bare metal
- `exit()` calls in main — should become an infinite loop or watchdog reset

---

## Build flag summary for 3.0

```
-DSMALL_TARGET          base tuning (NUM_VAR=130, CTL_DEPTH=24, SA_SIZE=80, MAX_FILES=4)
-DAVR_PROGMEM           enable PROGMEM for string tables (AVR only)
-DNO_BEEP               disable BEEP if no PWM available
-DNUM_VAR=52            minimal variable set if RAM is very tight
-DSA_SIZE=40            tighter string accumulator for very small RAM
```

---

## What 3.0 is NOT

- Not a bolt-on to the 2.x codebase
- Not just adding `#ifdef AVR` around things
- 2.x is feature-stable; 3.0 starts from the hardware and works backward
