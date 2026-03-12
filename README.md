# Enhanced Micro-Basic 2.2

A generative AI test to see if Claude.ai, using only the free plan (Sonnet 4.6),
could cross-port the single-file BASIC example from David Dunfield's Micro-C
compiler installer to GCC. We took this a few steps further and made it work with
MinGW and IA16-ELF to fully support 16-bit DOS all the way to modern times.
Development has continued into a fork — **Enhanced Micro-Basic** — adding new
language features while keeping the same philosophy of small, portable, and
single-file.

**This was a successful project.**

---

## What this is

An academic test, a fun project, a digital what-if.

## What this is not

An example of a modern, purpose-built BASIC in GCC.

## What we tried to do

Make something that could potentially be useful, the way that Micro-Basic was
useful in the past. By that we mean: it's small, does what it says on the tin,
and can be easily adapted to things like Z80 or 6809 natively in C without a
bunch of messing around.

---

## Licence

Based on source comments, Micro-Basic is free to use for personal use and is
still under copyright of David Dunfield. Files from his website are included to
help clarify the licence status. The basic position is: not cleared for
commercial resale, personal or educational use is fine.

Where does that leave this port? It's fine also for personal use. Though the
changes are significant to bring a lot of this to modern compilers, I would still
err on the side of caution — think of this port as like the Ship of Theseus.
This code still contains a lot of the reasoning, control flow, and in fact
specific syntax of David's original code; you can compare them side by side to
see that. A lot of the prototypes had to be completely redone because K&R assumes
much about the system it's running on, and GCC does not. I'll stop short of
saying it's in the public domain. As long as we keep to the spirit of the
original source as an example work and leave it at that, we're in the clear. All
new stubs written for this port are MIT licensed.

---

## Language Overview

This is integer BASIC. A brief summary is given below; the full command reference
is in the manual in the `/documentation` folder.

### Control flow

- `IF expr THEN stmt` — conditional single statement
- `LIF expr THEN stmts` — conditional rest of line
- `GOTO`, `GOSUB` / `RETURN`
- `FOR` / `NEXT`, `WHILE`-style loops via `IF` + `GOTO`

#### Relative jumps

Wherever a line number is accepted by `GOTO`, `GOSUB`, or `IF ... THEN`, you may write `+n` instead to jump forward *n* lines from the currently executing line (n = 1–127). This is useful for short forward branches that would otherwise need a named target line.

```basic
IF A = 0 THEN +2       : REM skip the next line if A is zero
PRINT "A is not zero"
PRINT "done"
```

Backward relative jumps (`-n`) are not supported and produce a syntax error. Line-counting always starts from the line containing the jump statement; `+1` skips one line forward.

#### Segments 
You can add program segments to a jump cache, which allows for faster jump refrence than direct / literal jumps with line numbers.  These can be combined with a relitive offset, either directly or with a variable creating a way to do complex jump tables and more complex logic based recursion.  This also provides a way of doing reverse refrence recursion.

a segment is defined (SEG [n] = line number):

```basic
1 REM *** USUAL USEAGE: ***  
2 SEG [1] = 1000             :REM Subroutine N+1
5 Let N = 1
10 PRINT "Count up: "
20 PRINT N
30 GOSUB [1]                 : REM SUB is N+1  Line 1000
40 IF N < 10 THEN GOTO 20    : REM IF N is not more than 10
50 END
60 REM **** SUBROUTINE N+1 ******
1000 LET N = N+1 
1001 RETURN    

```
But also allowing for making a jump table:

```basic
1 REM *** CODE SNIP FROM MANDELBROT EXAMPLE ***
2 REM *** EARLY IN PROGRAM SEG STATEMENTS: 
3 SEG [2] = 230   :REM Pixel Printer, represents a diferent level of recursion
4 SEG [3] = 390   :REM EXIT point, this could also be a relitive jump with manual calculation.

205  REM Later on...
210  O=I&7                  
220    GOTO [2]+O           :REM Pixel Printer Jump Table
230    PRINT " ";:GOTO [3]  :REM Pixel 0 - Jumps to line 390 to exit pixel printing
240    PRINT ".";:GOTO [3]
250    PRINT ":";:GOTO [3]
260    PRINT "-";:GOTO [3]
270    PRINT "+";:GOTO [3]
280    PRINT "*";:GOTO [3]
290    PRINT "#";:GOTO [3]
300    PRINT "@";
390  NEXT J                 :REM exit from pixel printing loop
```

### Hardware

- `BEEP freq, ms` — PC speaker tone (real-mode DOS targets)
- `INP(port)` / `OUT port, val` — I/O port access (real-mode DOS targets)
- All versions support `LOAD "FILENAME"` and `SAVE "FILENAME"` (extension added automatically, files are plain ASCII)

### Variables

| Kind | Names | Count |
|------|-------|-------|
| Numeric | `A0`–`A9` ... `Z0`–`Z9` | 260 |
| String | `A0$`–`A9$` ... `Z0$`–`Z9$` | 260 |
| Array | `A0()`–`A9()` ... `Z0()`–`Z9()` | 260 |

The `0` suffix may be omitted: `A` == `A0`, `Z$` == `Z0$`.

### Numeric type

All numeric values are stored as 16-bit quantities and interpreted as signed by default.

Operator precedence from highest to lowest:

```
Unary:   !  -              (bitwise NOT, unary minus)
         &  |  ^  <<  >>  (bitwise and bit shift)
         *  /  %           (multiplicative)
         +  -  =  <>  <  <=  >  >=   (additive and comparison — same priority)
```

> **Important:** addition, subtraction, and comparisons share the same priority
> and are evaluated left to right. This means:
>
> ```basic
> IF A + B = C + D THEN 100
> ```
>
> evaluates as `IF ((A + B) = C) + D THEN 100`, not as a comparison of two sums.
> Always parenthesise when mixing arithmetic and comparisons:
>
> ```basic
> IF (A + B) = (C + D) THEN 100
> ```

Bitwise operators sitting above comparisons is intentional — the most common use
case is masking before testing, which then requires no parentheses:

```basic
IF A & 15 = 7 THEN 100    : REM evaluates as (A & 15) = 7
```

Expressions may be nested up to 8 levels deep.

Unary `!` and unary `-` operate on the **unsigned** 16-bit representation of
their operand. The result is returned as signed. This ensures `!0` gives `65535`
(−1 as signed) and `-(-32768)` wraps to `-32768` rather than overflowing.

### Numeric literal prefixes

Integer literals are normally signed decimal. Two prefix characters allow
alternative input formats. All values are stored as 16-bit quantities and
interpreted as signed by default.

| Prefix | Type | Example |
|--------|------|---------|
| `#` | Hexadecimal | `#FF`, `#1A2B`, `A = #8000` |
| `@` | Unsigned decimal | `@65535`, `@32768` |
| none | Signed decimal | `255`, `-128` |

There is no binary prefix. Use hex for bit patterns: `#0F` instead of `%00001111`.

### Unsigned comparison functions

The `<`, `<=`, `>`, `>=` operators are signed. Use these functions when values
may exceed 32767:

| Function | Returns |
|----------|---------|
| `UGT(a, b)` | 1 if a > b unsigned, 0 otherwise |
| `ULT(a, b)` | 1 if a < b unsigned, 0 otherwise |

```basic
IF UGT(A, #7FFF) THEN PRINT "upper half"
```

### PRINT separators

Items in a PRINT list may be separated by `,` or `;`. A semicolon suppresses
the space that would otherwise appear before a numeric value. Neither implements
print zones. A trailing `,` or `;` suppresses the newline.

```basic
PRINT "X = "; X          : REM  no space between label and value
PRINT "A = "; A, "B = "; B  : REM  mix freely
```

Because values are stored as 16-bit quantities and interpreted as signed by default, two string functions let
you display values in alternative formats:

| Function | Output |
|----------|--------|
| `HEX$(n)` | Uppercase hexadecimal — e.g. `FF`, `1A2B` |
| `UNS$(n)` | Unsigned decimal — e.g. `65535` |

```basic
A = #FFFF
PRINT HEX$(A)          : REM  FFFF
PRINT UNS$(A)          : REM  65535
PRINT STR$(A)          : REM  -1
```

---

## Documentation

The `/documentation` folder contains:

- `MICRO-BASIC.bnf` — Backus-Naur Form grammar
- `MICRO-BASIC-Manual.docx` — full command reference and examples

## Building

See [`BUILD.md`](BUILD.md) for full instructions. Quick reference:

```
make linux       # GCC / Linux
make windows     # MinGW 32-bit cross
make windows64   # MinGW 64-bit cross
make dos         # ia16-elf-gcc, normal model
make dos-small   # ia16-elf-gcc, SMALL_TARGET (64 KB)
```

## Binaries

Pre-built binaries for testing are in the `/dist` directory.
