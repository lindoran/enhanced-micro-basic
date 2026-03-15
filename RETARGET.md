# RETARGETABLE BASIC — Portable Infrastructure Reference

This document provides the proven portability infrastructure from Enhanced
Micro-BASIC 2.3 for use as a foundation in a new retargetable implementation.
These pieces are pure portability scaffolding with no behavioral baggage from
the 2.x codebase — copy them forward as-is.

---

## 1. Numeric Type Aliases

The interpreter uses three semantic typedefs. All architecture-specific
retargeting happens here and nowhere else in the source.

```c
#include <stdint.h>

/* =======================================================================
 * Portable type aliases
 *
 * bint  : the native BASIC integer — signed 16-bit on all current targets.
 *         On Z80 (SDCC) or 6809 (GCC6809) this stays int16_t; the
 *         compiler's own int is also 16-bit there, but being explicit is
 *         safer across compilers.
 *
 * ubint : unsigned form of bint — for bit ops, array indices, and anywhere
 *         the value is known non-negative (line numbers, dim sizes, etc.).
 *
 * bptr  : must be wide enough to hold a data pointer on the target.
 *         The control stack stores both small bint values (step, limit,
 *         variable index) AND pointers (runptr, cmdptr), so bptr must be
 *         used throughout for any slot that holds either kind.
 *
 *   Von Neumann flat 16-bit (Z80, 6809, ia16 near model) : uint16_t
 *   Von Neumann flat 32-bit (ia16 far model, 32-bit host) : uint32_t
 *   64-bit host                                           : uint64_t
 *   AVR (Harvard, flat 16-bit data space)                 : uint16_t
 *   Hosted / unknown                                      : uintptr_t
 * ======================================================================= */
typedef int16_t   bint;      /* BASIC numeric type  — signed 16-bit          */
typedef uint16_t  ubint;     /* BASIC unsigned type — unsigned 16-bit         */
typedef uintptr_t bptr;      /* pointer-width slot  — control stack entries   */
```

**To retarget:** change only these three lines. Nothing else in the
interpreter should contain a raw `int`, `unsigned`, or pointer-width
assumption.

---

## 2. Token Type and Macros

Keyword tokens are stored as bytes with the high bit set, so they can
share a char stream with plain ASCII text without ambiguity.

```c
/* =======================================================================
 * tok_t — type for a single byte from a tokenised line.
 * Using a named type removes scattered (signed char) casts.
 *
 * TOKEN(k)  -> the byte stored in the token stream for keyword index k
 * IS_TOK(c) -> true if byte c is a token (high bit set / value negative)
 * ======================================================================= */
typedef signed char tok_t;
#define TOKEN(k)   ((tok_t)((k) | 0x80))
#define IS_TOK(c)  ((tok_t)(c) < 0)
```

Token values 1..127 are keyword indices. Bytes 0x00..0x7F in the token
stream are plain ASCII. Bytes 0x80..0xFF are keyword tokens. The NUL byte
(0x00) is always the end-of-line sentinel.

---

## 3. RODATA / Flash Address Space Abstraction

On Von Neumann targets (x86, Z80, 6809, ia16) there is one address space —
`const` data lands in ROM/flash automatically and normal pointer dereference
works. On Harvard targets (AVR) flash and RAM are separate buses; string
tables in flash must be read with `pgm_read_byte` / `pgm_read_word` or a
normal dereference reads RAM instead.

This macro layer makes the same source work on both without `#ifdef AVR`
scattered through the interpreter logic.

```c
/* =======================================================================
 * RODATA / RD_BYTE / RD_PTR — ROM vs RAM address space abstraction
 *
 * RODATA   : storage class annotation for tables that should live in flash.
 *            Von Neumann: expands to nothing (const + linker does the rest).
 *            Harvard/AVR: expands to PROGMEM.
 *
 * RD_BYTE(p)  : read one uint8 / char from a RODATA pointer.
 * RD_PTR(pp)  : read one (const char *) from a RODATA pointer-to-pointer.
 *               Used to walk string tables like reserved_words[] and
 *               error_messages[].
 *
 * Build flags:
 *   -DAVR_PROGMEM   : enable AVR PROGMEM path (also auto-enabled if __AVR__)
 *   -DTEST_RODATA   : force macro path on a hosted build for testing
 *                     (all macros collapse to plain dereferences, but every
 *                     access goes through the macro path so errors surface)
 * ======================================================================= */

#if defined(AVR_PROGMEM) || defined(__AVR__)
#  include <avr/pgmspace.h>
#  define RODATA          PROGMEM
#  define RD_BYTE(p)      pgm_read_byte(p)
#  define RD_PTR(pp)      ((const char *)pgm_read_word(pp))
#else
   /* Von Neumann / hosted: plain dereference, const goes to .rodata        */
#  define RODATA          /* nothing */
#  define RD_BYTE(p)      (*(const uint8_t *)(p))
#  define RD_PTR(pp)      (*(pp))
#endif
```

**Usage in string tables:**

```c
static const char * const RODATA reserved_words[] = {
    "PRINT", "FOR", "NEXT", /* ... */
    NULL
};

/* Walking the table — works identically on AVR and hosted: */
const char *w = RD_PTR(&reserved_words[i]);
while (RD_BYTE(w)) { /* ... */ ++w; }
```

---

## 4. Compile-Time String Helpers

Avoids `printf` for the banner and any other place an integer `#define`
needs to appear in a string at compile time.

```c
/* Stringify helpers — expand integer defines to string literals.
 * MKSTR(FORK_VER_MAJOR) -> "2"
 * Used in banner output to avoid any printf dependency.              */
#define XSTR(x) #x
#define MKSTR(x) XSTR(x)
```

**Usage:**

```c
#define VER_MAJOR 3
#define VER_MINOR 0
io_puts("My BASIC " MKSTR(VER_MAJOR) "." MKSTR(VER_MINOR) "\n");
```

---

## 5. Build-Time Size Guard

Enforce the relationship between buffer sizes at compile time rather than
discovering violations at runtime.

```c
/* SA_SIZE must be >= BUFFER_SIZE.
 * String literals are read into buffer[] first, then copied into the
 * string accumulator (sa1[SA_SIZE]).  If SA_SIZE < BUFFER_SIZE a long
 * literal silently overflows sa1.                                     */
#if SA_SIZE < BUFFER_SIZE
#  error "SA_SIZE must be >= BUFFER_SIZE (string literals are buffered first)"
#endif
```

---

## 6. Hand-Rolled Number Formatters

Replace `sprintf` entirely. No printf dependency, no formatter library,
each function is a handful of lines. All three use the same reverse-stack
pattern.

```c
/* num_string() — signed bint to decimal ASCII.
 * buf must be at least 7 bytes (-32768 + NUL).                       */
static void num_string(bint value, char *ptr)
{
    char  cstack[6];
    int   cptr = 0;
    ubint uval;
    if (value < 0) { *ptr++ = '-'; uval = (ubint)(-(int)value); }
    else           {               uval = (ubint)value; }
    do { cstack[cptr++] = (char)(uval % 10 + '0'); } while ((uval /= 10) != 0);
    while (cptr) *ptr++ = cstack[--cptr];
    *ptr = '\0';
}

/* uns_string() — ubint to unsigned decimal ASCII.
 * buf must be at least 6 bytes (65535 + NUL).                        */
static void uns_string(ubint value, char *ptr)
{
    char cstack[5];
    int  cptr = 0;
    do { cstack[cptr++] = (char)(value % 10 + '0'); } while ((value /= 10) != 0);
    while (cptr) *ptr++ = cstack[--cptr];
    *ptr = '\0';
}

/* hex_string() — ubint to uppercase hex ASCII (no prefix).
 * buf must be at least 5 bytes (FFFF + NUL).                         */
static void hex_string(ubint value, char *ptr)
{
    char  cstack[4];
    int   cptr = 0;
    do {
        ubint digit = value & 0xF;
        cstack[cptr++] = (char)(digit < 10 ? digit + '0' : digit - 10 + 'A');
        value >>= 4;
    } while (value);
    while (cptr) *ptr++ = cstack[--cptr];
    *ptr = '\0';
}
```

---

## 7. Bounded String Helpers

Replace `strcpy` / `strcat` / `snprintf` for string operations where the
destination size is known. Always NUL-terminate, never overflow.

```c
/* safe_copy() — bounded copy, always NUL-terminates dst.
 * Replaces strncpy (which does not guarantee NUL) and snprintf("%s"). */
static void safe_copy(char *dst, const char *src, ubint dstsize)
{
    ubint i = 0;
    if (!dstsize) return;
    while (src[i] && i < dstsize - 1) { dst[i] = src[i]; i++; }
    dst[i] = '\0';
}

/* safe_cat() — bounded append, always NUL-terminates dst.            */
static void safe_cat(char *dst, const char *src, ubint dstsize)
{
    ubint i = 0;
    if (!dstsize) return;
    while (dst[i] && i < dstsize - 1) i++;     /* find end of dst     */
    while (*src   && i < dstsize - 1) dst[i++] = *src++;
    dst[i] = '\0';
}
```

---

## 8. Workspace Interface

BASIC's entire memory world is a flat array provided by `io.h`.
BASIC declares these as extern — `io.h` / `io_stdio.c` / `io_avr.c`
defines them. BASIC never allocates, never calls malloc, never touches
a linker symbol directly.

```c
/* In basic.c — BASIC's view of its memory world. That's all it sees. */
extern uint8_t  *basic_workspace;
extern uint16_t  basic_workspace_size;
```

**Hosted implementation (`io_stdio.c`):**

```c
#include "io.h"

#ifndef WORKSPACE_SIZE
#  define WORKSPACE_SIZE  16384   /* override at build time */
#endif

static uint8_t _workspace[WORKSPACE_SIZE];

uint8_t  *basic_workspace      = _workspace;
uint16_t  basic_workspace_size = (uint16_t)WORKSPACE_SIZE;
```

**Bare metal implementation (`io_avr.c` or similar):**

```c
#include "io.h"
#include "bios.h"

/* BIOS arranges the workspace — internal RAM, external RAM, banked
 * window, whatever the target supports.  BASIC sees only the pointer
 * and the size.  Banking, paging, and physical address are BIOS
 * concerns, never BASIC's.                                           */
uint8_t  *basic_workspace      = NULL;   /* set by io_init() */
uint16_t  basic_workspace_size = 0;

void io_init(void)
{
    basic_workspace      = bios_workspace_ptr();
    basic_workspace_size = bios_workspace_size();
    /* ... rest of IO initialisation ... */
}
```

**What io.h must guarantee:**
- `basic_workspace` is non-NULL and writeable before any interpreter
  function is called
- `basic_workspace_size` accurately reflects writeable bytes available
- The workspace does not overlap interpreter static data or the stack
- On banked targets BIOS manages bank state transparently — BASIC always
  sees a consistent flat window through the pointer

**What BASIC must guarantee:**
- All dynamic allocation uses offsets into `basic_workspace` only
- Nothing outside `[basic_workspace .. basic_workspace + basic_workspace_size)`
  is ever written
- `FRE()` reports free bytes within the workspace, not total system RAM

---

## Notes on Usage

- Items 1-3 are the core portability layer — they belong at the top of
  `basic.c` before any interpreter code.
- Items 4-8 are utility and interface infrastructure — no library
  dependencies, safe to use on any target including bare metal.
- None of these items depend on `stdio.h`, `malloc`, or any other library.
- The entire set compiles cleanly under `-std=c99 -Wall -Wextra` on GCC,
  Clang, SDCC, and AVR-GCC.
