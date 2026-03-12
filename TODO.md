# ENHANCED MICRO-BASIC — TODO

---

## 2.3 — COMPLETED

### Memory safety pass 
All critical and high-priority safety issues resolved:

- **GOSUB/FOR stack overflow** — guard added before push (`ctl_ptr + N > CTL_DEPTH`)
- **RETURN/NEXT stack underflow** — guard added before pop
- **`get_var()` out of bounds** — index clamped to `NUM_VAR` on all build sizes
- **READ NULL deref** — auto-finds first DATA line if no ORDER issued; NULL check on `Llink` advance
- **`strcat` overflow in string concat** — length check before combining, error(12) on overflow
- **`SA_SIZE < BUFFER_SIZE`** — enforced as a compile-time `#error`; SMALL_TARGET default corrected
- **`eval_sub` local stacks** — bounds guard added; depth analysis documented in comment
- **`get_char_value` literal loop** — length guard added; invariant documented
- **argv copy was dead code** — `if (char_vars[j])` always false on zero-init static; fixed to
  `allocate()` + `strcpy`; added `j < NUM_VAR` bound
- **`concat()` into filename** — replaced with `safe_copy()` + `safe_cat()`; old `concat()`
  function removed entirely
- **`strcpy(buffer, sa1)` in OPEN** — replaced with `safe_copy()` now that SA_SIZE >= BUFFER_SIZE

### Library / printf elimination 
All `printf`, `fprintf`, `sprintf`, `snprintf` calls removed from the codebase.
Replaced with:
- `hex_string()` — hand-rolled ubint to uppercase hex, replaces `sprintf("%X",...)`
- `uns_string()` — hand-rolled ubint to unsigned decimal, replaces `sprintf("%u",...)`
- `put_uint()` — writes ubint to any FILE stream, replaces `fprintf(fp, "%u", ...)`
- `safe_copy()` / `safe_cat()` — bounded string copy/append, replace all `snprintf` filename patterns
- Banner reduced to a single `fputs` of compile-time string literals via `MKSTR()` macros

`printf` is confirmed absent from the linked binary (`nm` verified).
Remaining stdio dependency is `fgets`, `fopen`, `fclose`, `fputs`, `putc`, `fflush` — all simple
stream ops that are straightforward to stub for 3.0.

### DOS target clarified 
SMALL_TARGET is **not** a DOS build flag. A standard PC DOS environment has 256-640K of
conventional memory — ample for the normal build. The `dos-small` Makefile target has been
removed. SMALL_TARGET is reserved exclusively for the 3.0 embedded port.

`.com` vs `.exe` investigated: not viable without shedding ~10K of interpreter code,
which would require removing features. Not pursued. DOS target stays `.exe`.

### Makefile updated 
- Version bumped to 2.3
- `dos-small` target removed
- Double `all:` definition fixed — `all` = linux default, `build-all` = all platforms
- BNF grammar file now staged into every distribution package
- SMALL_TARGET scoped comment added

---

## Open — 2.x candidates

These items are within scope for a future 2.x release but require design decisions
before implementation. None are blocking for 2.3.

### Lexer table simplification
Keyword scan could stop at 3-4 characters (like MS BASIC) rather than matching full
keyword strings. Benefits: smaller token tables, less memory on small targets.
Related question: error messages as plain text vs numeric codes — plain text is
currently in `.rodata`; codes with an offline message table would save RAM on
embedded targets but is a 3.0 concern, not 2.x.

### Symbol shorthand
- `?` as alias for PRINT
- `;` for comments (no `:` separator required)
- Comment lines dropped by scanner/lexer, never stored

### Protected range array addressing
A memory window type: pointer + size, like a DIM variable but mapping to a fixed
address range. Useful for small targets where display RAM or device registers are
memory-mapped. Bounds checking via the declared window size.
- Window interior limited to 16-bit reference; pointers are actual addresses
- On hosted targets would tie back into malloc for bounds checking
- On bare metal the limits are target-specified

### Compressed / symbol program storage
Deferred from 2.3 — architectural change, not a patch.
Store tokenised symbols in RAM, plaintext only on disk. LIST/SAVE reconstitute
plaintext from the symbol stream. REM text and keyword fulltext stored offline.
Large targets could host both forms depending on build configuration.
This is closely related to 3.0 workspace model decisions — better addressed there.

### Line number elimination
CBA (cost/benefit analysis needed). Line numbers currently serve as the sort key
for the linked list and the target for GOTO/GOSUB resolution. Removing them means
a label/symbol table. Non-trivial. Likely a 3.0 decision given other changes.

---

## Deferred to 3.0 — Embedded / Bare Metal Port

3.0 targets a specific small device (ATmega2560 or similar). It is **not** a
bolt-on to the current codebase — it is a fresh design starting from the hardware
constraints and working backward.

### I/O HAL replacement
The remaining stdio dependency (`fgets`, `fopen`, `fclose`, `fputs`, `putc`, `fflush`)
must be replaced with a target HAL for bare metal. A serial-only build (no file I/O)
is the expected first 3.0 configuration. See **PORTING.md** for the full checklist.

### `TODO(fre)` — FRE() function
`FRE()` requires a meaningful free heap figure. On Linux/Windows this is pointless.
On ia16 DOS the newlib heap accounting does not reflect conventional memory correctly.
On bare metal (AVR, Z80) malloc is a simple bump allocator into known RAM and `FRE()`
would work correctly and be genuinely useful.

Implement in 3.0. At that point the workspace model may change to a fixed allocation
at startup rather than dynamic malloc throughout, making `FRE()` trivial pointer
arithmetic rather than a heap query.

### RNG for 8-bit targets
`rand()` is not available on bare metal. Replace with an XOR-shift generator.
Will need a seed stub appropriate to the target (no `time(NULL)` on AVR).
See `TODO: XOR SHIFT` comment in `get_value()`.

### Workspace model
Decide between linked list program storage (current) vs flat buffer for bare metal.
Current model uses `malloc` throughout — not viable on a fixed-RAM target without
a bump allocator. Full variable set (260 numeric, 260 string, 260 array) may not
be realistic depending on RAM budget. See PORTING.md.

### General 3.0 questions
- What fits in flash
- What the RAM budget actually is on the chosen target
- Whether the linked list program storage makes sense or a flat buffer is better
- Whether the full variable set is realistic
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