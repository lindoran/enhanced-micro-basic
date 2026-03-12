# MICRO-BASIC 2.2 — TODO

Items are grouped by complexity and listed in recommended implementation order.
Each item references the tag used in source comments for easy searching.

## Optimization pass
Perform a optimization pass for 2.3 to decrease size of program, and get ready for 
smaller targets and 3.0

memory safety with a low cost: 
 - do what we can to improve memory safety by changing code in low cost ways
 - check for bugs, garanteed overflows etc.
 - look for buffer size ineficencies
 - ernest is still on the programmer to be smart, but gardrail clifs.

look at library inclusion (especially in ia16)
 - can we code around single use case? (safely!)
 - look for ways to save on runtime size (its about a 50K exe file!)
 - look for runtime optimizaitons that are low cost, in terms of program size.
 - look for common theam system buffers we can use from the OS that cost nothing, 
   variable space, entry buffers etc.. call out these where we have to replace
   them on other platforms.
 - start a seprate .md for Embeded/ROM acception so we can work twards a mark
   list for what is needed to port. 
 - .com vs .exe on ia16 small target?

lexer table look ups could be simplified to only scan 3 - 4 characters like MS basic does
 - smaller tables, less memory
 - error plaintext vs codes (offline storage on small targets?)

can save the program as symbols vs full text, and only display:
 - comments from offline storage as needed
 - symbol fulltext from offline storage as needed (LIST, SAVE etc ...)
 - means a offline program space would be required to define after first numbered line is entered
   or the system would have to load a blank workspace, like office does and then save
   would copy from it.
 - text line buffer stores a number of lines defined by build before cashed to disk
   - This can be adjuseted in the interperater.
 - symbols in memory for runtime, offline is plaintext and is converted when loaded.
 - large targets could host both a plaintext file, and a codespace depending on how 
   these offsets are configured.

symbol shorthand
 - '?' for print etc ';' comments 
 - comments dont require a : to seperate or diliminate when stored this way
 - line drops in scanner / lexer when encountered.

Look at eliminating line numbers?  CBA (what is the reason they are there, scanning etc...)

Protected range array addressing
 - define a memory segment as a pointer and its size (like a dim varable) 
 - can be used like POKE and PEEK is used but with guard rails.  This specifically will need to tie back into
   malloc for bounds checking etc OR the limits will need to be specified by the target.  
 - this is usefull for small targets were display area is a file in memory, or direct device access. 
 - inside limit is within a 16 bit refrence, but pointers are actual locations (ie the window can only be 16 bits big)


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
