# PORTING NOTES — Embedded / Bare Metal (3.0)

This document tracks what needs to change to port Enhanced Micro-BASIC to a
bare metal target (ATmega2560 or similar). It is a working checklist, not a
design spec. Items will be promoted to proper design decisions when 3.0 work begins.

---

## I/O Architecture — Two Layer Design

The I/O abstraction is split into two layers. `BASIC.c` never touches hardware
or stdio directly — it only calls `io.h`. This preserves the single source goal:
`BASIC.c` is identical across all targets.

```
BASIC.c
   |
   v
io.h          <- interpreter's ceiling; all I/O calls go through here
   |
   +-- io_stdio.c    (hosted: Linux / Windows / DOS — wraps stdio.h)
   |
   +-- io_avr.c      (bare metal: wraps the BIOS layer below)
   |      |
   |      v
   |   bios.h        <- hardware abstraction for the specific board
   |      |
   |      +-- uart.c
   |      +-- sdcard.c
   |      +-- display.c
   |      +-- keyboard.c
   |
   +-- io_stub.c     (no-op stub for bringing up a new port)
```

The Makefile selects the right io_xxx.c per target. The BIOS layer is
board-specific and reusable beyond just BASIC — other software on the same
hardware uses the same BIOS. This is the same separation CP/M used: BDOS was
the OS, BIOS was the hardware-specific part, applications called BDOS only.

---

## io.h — The Interpreter's I/O Interface

The full set of I/O operations the interpreter needs is small. Every call site
in BASIC.c is already tagged `HAL(3.0):` — grep for that to find them all.

```c
/* io.h - Enhanced Micro-BASIC I/O abstraction layer */

/* Console output */
void      io_putc(char c);
void      io_puts(const char *s);
void      io_flush(void);                  /* no-op on serial */

/* Console input */
int       io_getline(char *buf, int max);  /* returns 0 on EOF/error */

/* File I/O — stub to no-ops on bare metal without filesystem */
typedef struct io_file IO_FILE;
IO_FILE  *io_fopen(const char *name, const char *mode);
void      io_fclose(IO_FILE *fp);
void      io_fputs(const char *s, IO_FILE *fp);
void      io_fputc(char c, IO_FILE *fp);
int       io_fgetline(char *buf, int max, IO_FILE *fp);

/* System */
void      io_exit(int code);               /* exit() or watchdog reset */
```

`IO_FILE *` on hosted builds is a thin wrapper around `FILE *`.
On bare metal without a filesystem all `io_fopen` / `io_fclose` / `io_fgetline`
calls return NULL / no-op. LOAD and SAVE become serial operations (plain text
paste or XMODEM) implemented in `io_avr.c`, not the interpreter.

---

## BIOS Layer — Board Specific

The BIOS layer sits below `io_avr.c` and is designed around the specific
hardware. For an ATmega with a video/keyboard hat this would include:

**UART** — console I/O, serial LOAD/SAVE
- `uart_tx(char c)` / `uart_rx_ready()` / `uart_rx()`
- Line buffering for `io_getline`
- Used as fallback LOAD/SAVE if no SD card present

**SD card** — filesystem for LOAD/SAVE/OPEN
- SPI driver + FAT library (FatFs is the standard choice for AVR)
- `io_fopen` / `io_fclose` / `io_fgetline` map to FatFs calls in `io_avr.c`
- SD presence can be runtime-detected; fall back to serial if absent

**Display** — for video hat
- Character write to framebuffer maps to `io_putc`
- If the hat has its own controller (SSD1306, TMS9918, etc.) this is a
  register write; if it's a direct framebuffer it maps to the protected
  range array addressing feature (see TODO.md)
- Scroll, cursor, clear are BIOS concerns — the interpreter just writes chars

**Keyboard** — for keyboard hat
- Scan code to ASCII translation in the BIOS
- Non-blocking `kbtst()` for the KEY() function maps to a BIOS ring buffer

**The interpreter never knows which BIOS functions exist.** `io_avr.c` is the
only file that calls BIOS functions. If the SD card is absent, `io_avr.c`
routes LOAD/SAVE to UART. BASIC.c sees only `io_fopen()` returning NULL.

---

## Layer Contracts

Each boundary in the stack has a contract — what the upper layer expects,
what the lower layer promises, and what neither side should assume about the
other. These are capability agreements, not function signatures. The actual
function names come later; get the contract right first.

The initial `io_stdio.c` implementation is valuable precisely because writing
it forces these contracts to become concrete. Any place you reach for something
stdio provides that `io.h` doesn't define yet is a gap in the contract.

---

### BASIC ↔ IO

**BASIC expects IO to provide:**
- A way to send a character or string to the console
- A way to read a complete line of input from the console
- A way to open a named stream for reading or writing (may return failure)
- A way to read a line or write a character to an open stream
- A way to close a stream
- A way to flush pending output
- A way to terminate the interpreter cleanly

**IO promises BASIC:**
- Console output will not silently drop characters
- A failed open returns a clear failure value, never a broken handle
- Reading from a closed or failed stream returns a defined empty/error result,
  never undefined data
- Flush is always safe to call, even if it is a no-op on the target
- Exit is always safe to call — on bare metal this means reset or halt,
  never a crash

**Neither side assumes:**
- BASIC does not assume a filesystem exists — it checks the return value of open
- BASIC does not assume the console is a terminal — it never sends escape codes
  or cursor control directly; that is IO's concern if the target supports it
- IO does not assume BASIC will close every stream it opens — targets must
  handle cleanup on exit/reset without relying on BASIC to be tidy
- IO does not assume unbounded line length — it is always given a buffer and
  a max size and must respect both

---

### IO ↔ BIOS

**IO expects BIOS to provide:**
- A way to transmit a single character (blocking acceptable)
- A way to test whether an input character is waiting (non-blocking)
- A way to receive a single character (blocking)
- A way to read and write named storage if the target has it (SD, flash, etc.)
- A way to detect whether storage is present at runtime
- A timer tick or delay primitive
- A hardware tone primitive if BEEP is supported

**BIOS promises IO:**
- Character tx will not silently drop data under normal operation
- The ready/receive pair is consistent — if ready returns true, receive
  will return a character without blocking
- Storage open/read/write/close form a matched set; a failed open means
  subsequent read/write calls will not be made on that handle
- Storage presence detection is reliable at the time of the call — BIOS
  does not promise the card stays present after detection
- Delay is at least as long as requested; may be longer on coarse-grained
  timers, never shorter

**Neither side assumes:**
- IO does not assume storage is always present — it detects at runtime and
  falls back to serial gracefully
- IO does not assume a specific storage interface — BIOS abstracts SD, flash,
  or anything else behind the same read/write/open/close calls
- BIOS does not assume IO will handle partial reads — it delivers complete
  lines or clearly indicates a short read
- BIOS does not assume a specific baud rate or terminal type — character I/O
  is raw bytes; framing and encoding are IO's concern
- BIOS does not assume it knows the difference between console and file streams
  — that distinction belongs to IO

---

### BIOS ↔ Hardware

**BIOS expects hardware to provide:**
- Behaviour consistent with the datasheet
- Stable power before init is called
- Defined reset state at startup

**BIOS promises IO:**
- All hardware is initialised before any BIOS function is called by IO
- Failures that can be detected (no SD card, UART framing error) are
  reported cleanly rather than returning garbage
- Hardware that is absent or unsupported returns a defined not-present
  result, not a hang

**Neither side assumes:**
- BIOS does not assume peripherals are always present — it probes and reports
- BIOS does not assume a specific clock speed — timing-sensitive code uses
  the actual F_CPU value, not a hardcoded constant
- Hardware does not need to know anything about BASIC, IO, or the layers
  above — BIOS is the only thing that touches registers directly

---

## Workspace Model — The Fundamental Principle

This is the most important architectural decision for 3.0 and it must be
stated clearly so the interpreter is never written to violate it.

**BASIC sees exactly one thing: a flat array with a size.**

```c
/* Everything BASIC knows about its memory world */
extern uint8_t  *basic_workspace;
extern uint16_t  basic_workspace_size;
```

`io.h` provides these two symbols before `main()` runs. That is all.

BASIC does not know and must never assume:
- Whether the array is in internal RAM or external RAM
- Whether it is banked, paged, or flat
- What the physical address is
- What the linker script looks like
- Whether malloc exists on the target
- How much RAM the target has in total

All memory topology decisions live in `io.h` and the BIOS layer:

```
io_stdio.c (hosted):
    static uint8_t _workspace[WORKSPACE_SIZE];
    uint8_t  *basic_workspace      = _workspace;
    uint16_t  basic_workspace_size = WORKSPACE_SIZE;

io_avr.c (bare metal, internal RAM):
    /* BIOS has already reserved this block */
    uint8_t  *basic_workspace      = bios_workspace_ptr();
    uint16_t  basic_workspace_size = bios_workspace_size();

io_avr.c (bare metal, banked external RAM):
    /* BIOS manages bank switching; hands BASIC a window */
    uint8_t  *basic_workspace      = bios_workspace_ptr();
    uint16_t  basic_workspace_size = bios_workspace_size();
```

From BASIC's perspective these three cases are identical. The banking
logic, the linker script, the external RAM initialisation — none of that
is BASIC's problem. BASIC just uses the array it was given.

### Two regions within the workspace

Within that flat array BASIC manages two regions itself using the
two-direction heap model. This is an internal BASIC concern — io.h does
not need to know about it:

```
basic_workspace[0]                    basic_workspace[size-1]
|                                                            |
+------------------------------------------------------------+
|  program store + DIM arrays  -->         <-- string heap  |
|  heap_lo (grows up)               heap_hi (grows down)    |
+------------------------------------------------------------+
         |                                  |
         +------------ FREE ----------------+
                  FRE() = heap_hi - heap_lo
```

### What io.h must guarantee about the workspace

These are the workspace-specific additions to the IO↔BASIC contract:

- `basic_workspace` is valid and writeable before any interpreter function
  is called
- `basic_workspace_size` accurately reflects the number of bytes available
- The workspace does not overlap with the interpreter's own static data
  (control stack, variable tables, string accumulators, input buffer)
- The workspace does not overlap with the stack
- On banked targets: the workspace pointer is valid for the lifetime of
  the interpreter — if banking is in use, io.h/BIOS manages the bank
  state transparently so BASIC always sees a consistent flat window

### Interpreter private RAM is separate

The interpreter's own static data — control stack, variable pointer tables,
string accumulators `sa1`/`sa2`, input `buffer[]`, `filename[]` — is NOT
part of the workspace. It lives in normal `.bss` / `.data` and is sized
entirely by build flags. The programmer never sees it and `FRE()` does not
count it.

This means the total RAM budget for a target is:

```
total RAM = interpreter static overhead + basic_workspace_size + stack
```

The interpreter static overhead is calculable at build time from the build
flags. Document it per target so the programmer knows honestly how much
of the chip's RAM `basic_workspace_size` can actually be.

---

## Memory — No malloc

### Two-direction heap (recommended for 3.0)

A single fixed RAM block above the static interpreter data. Program lines and
arrays grow up from the bottom; string content grows down from the top.
They meet in the middle only on genuine OOM — no fragmentation because neither
side ever has holes. `FRE()` is one subtraction.

```
+---------------------------+  <- heap_base (just above static data)
|  program lines            |
|  DIM arrays               |
|  vvvvvvvvvvvvvvvvvvvvvvv |  <- heap_lo (grows up)
|                           |
|      FREE                 |  FRE() = heap_hi - heap_lo
|                           |
|  ^^^^^^^^^^^^^^^^^^^^^^^^^|  <- heap_hi (grows down)
|  string content           |
+---------------------------+  <- heap_top
|  stack                    |  <- grows down, fixed size, below heap
+---------------------------+  <- top of RAM
```

**String reassignment** — old content is orphaned until CLEAR/NEW resets
`heap_hi` to `heap_top`. This is acceptable for typical BASIC programs.
The common `A$ = A$ + "x"` case can be optimised: if the old string is the
most recently allocated (i.e. right at `heap_hi`), reclaim it before
allocating the new one. Handles the loop case cleanly.

**Program lines** — on a 3.0 target with no interactive editing, the program
region fills once on LOAD and never fragments. A flat buffer is simpler than
the current linked list for this use case.

### Banked RAM

On systems with banked RAM (Z80, some AVR + external RAM configurations)
keep interpreter state, variable tables, and string heap in fixed lower RAM.
Use the banked window exclusively for program line storage — it's the biggest
consumer and is accessed sequentially, making bank boundaries manageable.

The segment cache (`SEG [n] = lineno`) is already the right mechanism for
bank-aware jump resolution. Extend `line_rec` with a bank tag and make
`runptr` a (bank, offset) pair rather than a flat pointer. Every place that
dereferences `runptr->Ltext` pages in the correct bank first — that's the
only change needed in the interpreter. The rest lives in `io_avr.c` / BIOS.

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

## HAL functions (existing — in BASIC.c platform block)

These are already isolated behind the platform HAL block in BASIC.c:

| Function | Current (DOS/Linux) | Bare metal need |
|---|---|---|
| `do_beep(freq, ms)` | PC speaker / ALSA | PWM tone on a timer |
| `do_delay(ms)` | `nanosleep` / BIOS tick | `_delay_ms()` or timer |
| `kbtst()` | termios non-blocking read | UART RX non-blocking |
| `do_in(port)` / `do_out(port, val)` | no-op (Linux/Win) | AVR `_SFR_IO8` or direct port |

On bare metal `kbtst()` reads from the BIOS keyboard ring buffer.
`do_beep` maps to BIOS PWM. `do_delay` maps to BIOS timer.

---

## Source call sites to convert (HAL(3.0) tags)

All remaining stdio call sites in BASIC.c are tagged `/* HAL(3.0): ... */`.
Run `grep HAL BASIC.c` to get the full list. Converting them means replacing
the stdio call with the corresponding `io.h` function.

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
- The BIOS layer is a separate project — design it for the specific board,
  not for BASIC specifically; other software should be able to use it too

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
