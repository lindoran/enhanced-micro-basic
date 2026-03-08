/*
 * ENHANCED MICRO-BASIC 2.2
 *
 * A small INTEGER BASIC interpreter originally written by Dave Dunfield,
 * subsequently ported to MICRO-C, then modernized for GCC / ia16-elf-gcc
 * / MinGW (2026).
 *
 * Variables:
 *   260 Numeric   variables : A0-A9 ... Z0-Z9
 *   260 Character variables : A0$-A9$ ... Z0$-Z9$
 *   260 Numeric   arrays    : A0()-A9() ... Z0()-Z9()
 *
 *   The '0' suffix may be omitted: A == A0, Z$ == Z0$
 *
 * Statements:
 *   BEEP freq,ms              Generate a tone on the PC speaker
 *   CLEAR                     Erase variables only
 *   CLOSE#n                   Close file (0-9) opened with OPEN
 *   DATA                      Inline data for READ
 *   DELAY ms                  Pause execution
 *   DIM var(size)[,...]        Dimension an array
 *   DOS "command"              Execute an OS shell command
 *   END                       Terminate program silently
 *   EXIT                      Quit MICRO-BASIC
 *   FOR v=init TO limit [STEP n]  Counted loop
 *   GOSUB line                Call subroutine
 *   GOTO  line                Unconditional jump
 *   IF test THEN line         Conditional jump
 *   IF test THEN stmt         Conditional single statement
 *   INPUT [prompt,] var       Read a value
 *   INPUT#n, var              Read from file
 *   LET (default)             var = expression
 *   LIF test THEN stmts       Long IF (rest of line)
 *   LIST [start[,end]]        List source lines
 *   LIST#n ...                List to file
 *   LOAD "name"               Load program from disk
 *   NEW                       Clear program and variables
 *   NEXT [v]                  End FOR loop
 *   OPEN#n,"name","mode"      Open file (fopen modes)
 *   ORDER line                Set READ data pointer
 *   OUT port,expr             Write I/O port
 *   PRINT [expr[,...]]        Print to console
 *   PRINT#n,...               Print to file
 *   READ var[,...]            Read from DATA statements
 *   REM                       Comment
 *   RETURN                    Return from GOSUB
 *   RUN [line]                Run program
 *   SAVE ["name"]             Save program to disk
 *   STOP                      Halt with message
 *
 * Operators:
 *   + - * / %                 Arithmetic (+ also concatenates strings)
 *   & | ^                     Bitwise AND, OR, XOR
 *   = <>                      Equal / not-equal (numeric or string)
 *   < <= > >=                 Comparisons (numeric only)
 *   !                         Unary bitwise NOT
 *   Comparison operators evaluate to 1 (true) or 0 (false).
 *
 * Numeric literal prefixes (Enhanced Micro-Basic):
 *   #xxxx                     Hexadecimal  e.g. #FF, #1A2B
 *   @dddd                     Unsigned decimal  e.g. @65535, @32768
 *   (none)                    Signed decimal  e.g. 255, -128
 *
 * Functions:
 *   ABS(n)     Absolute value
 *   ASC(s)     ASCII value of first character
 *   CHR$(n)    Single character from ASCII value
 *   HEX$(n)    Convert number to uppercase hex string (e.g. FF, 1A2B)
 *   INP(port)  Read I/O port
 *   KEY()      Non-blocking keyboard test
 *   NUM(s)     Convert string to number
 *   RND(n)     Random number 0..n-1
 *   STR$(n)    Convert number to string
 *   UNS$(n)    Convert number to unsigned decimal string (e.g. 65535)
 *
 * Copyright 1982-2003 Dave Dunfield  -  all rights reserved.
 * Permission granted for personal (non-commercial) use only.
 *
 * Modernization notes (2026):
 *   - Explicit semantic typedefs (bint/ubint/bptr) replace raw int/unsigned.
 *     To retarget (Z80/SDCC, 6809/GCC6809, etc.) adjust the typedef block
 *     below only; no other changes are needed for the numeric types.
 *   - All functions have explicit return types and forward declarations.
 *   - Token bytes handled via tok_t type and TOKEN(x) macro throughout.
 *   - Platform HAL (#ifdef block) isolates BEEP/DELAY/KEY/INP/OUT.
 *   - File modes "rv"/"wv" -> "rb"/"wb" (Micro-C verbatim -> standard).
 *   - concat(), random() replaced with local/standard equivalents.
 *   - fgets() CR/LF stripping added (Micro-C I/O stripped these implicitly).
 *   - num_address()/str_address() replace the unsafe uintptr_t* trick.
 *
 * Build:
 *   GCC/Linux : gcc -std=c99 -Wall -O2 -o basic basic.c
 *   MinGW     : gcc -std=c99 -Wall -O2 -o basic.exe basic.c
 *   ia16/DOS  : ia16-elf-gcc -mcmodel=small -O2 -o basic.exe basic.c -li86
 *   ia16 small: ia16-elf-gcc -mcmodel=small -O2 -DSMALL_TARGET -o basic.exe basic.c -li86
 *   Micro-C   : cc basic -fop
 *
 * Small-target tuning:
 *   -DSMALL_TARGET          enables conservative defaults for 64 KB targets
 *   Individual overrides:   -DNUM_VAR=52 -DCT_DEPTH=12 -DSA_SIZE=32 etc.
 */

/* =======================================================================
 * Version identification
 *
 * Bump FORK_VER_MINOR on each Enhanced Micro-Basic release.
 * FORK_VER_MAJOR resets FORK_VER_MINOR to 0.
 * BASE_VER_* tracks the upstream Dunfield/modernisation version being
 * forked from — update only when rebasing on a new upstream drop.
 *
 * The banner in main() reads exclusively from these defines.
 * Nothing else in the source should contain a version number string.
 * ======================================================================= */
#define FORK_NAME        "Enhanced Micro-Basic"
#define FORK_VER_MAJOR   2
#define FORK_VER_MINOR   2
#define BASE_VER_STR     "Micro-Basic 2.1"
#define BUILD_YEAR       "2026"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <setjmp.h>
#include <stdint.h>

/* =======================================================================
 * Portable type aliases
 *
 * bint  : the native BASIC integer - signed 16-bit on all current targets.
 *         On Z80 (SDCC) or 6809 (GCC6809) this stays int16_t; the
 *         compiler's own int is also 16-bit there, but being explicit is
 *         safer across compilers.
 *
 * ubint : unsigned form of bint - for bit ops, array indices, and anywhere
 *         the value is known non-negative (line numbers, dim sizes, etc.).
 *
 * bptr  : must be wide enough to hold a data pointer on the target.
 *         The control stack stores both small bint values (step, limit,
 *         variable index) AND pointers (runptr, cmdptr), so it must use
 *         bptr throughout.
 *         - DOS/ia16 near model : uint16_t  (same as ubint, flat 64 KB)
 *         - DOS/ia16 far model  : uint32_t
 *         - 32-bit hosts        : uint32_t  (via uintptr_t)
 *         - 64-bit hosts        : uint64_t  (via uintptr_t)
 *         For a Z80 flat space  : typedef uint16_t bptr;
 * ======================================================================= */
typedef int16_t   bint;     /* BASIC numeric type   - signed 16-bit         */
typedef uint16_t  ubint;    /* BASIC unsigned type  - unsigned 16-bit        */
typedef uintptr_t bptr;     /* pointer-width slot   - ctl_stk entries        */

/* =======================================================================
 * tok_t - the type for a single byte read from a tokenised line.
 * Using a named type removes all the scattered (signed char) casts.
 *
 * TOKEN(k)  -> the byte stored in the token stream for keyword index k
 * IS_TOK(c) -> true if byte c is a token (high bit set / value negative)
 * ======================================================================= */
typedef signed char tok_t;
#define TOKEN(k)   ((tok_t)((k) | 0x80))
#define IS_TOK(c)  ((tok_t)(c) < 0)

/* =======================================================================
 * Platform detection & Hardware Abstraction Layer
 * Exports: do_beep(freq,ms)  do_delay(ms)  kbtst()  do_in(p)  do_out(p,v)
 * ======================================================================= */

#if defined(__ia16__) || defined(__MSDOS__) || defined(_MSDOS)
/* -----------------------------------------------------------------------
 * Real DOS: ia16-elf-gcc + libi86, DJGPP, Open Watcom, Turbo C, etc.
 *
 * Port I/O: libi86 and Open Watcom use outp()/inp() from <conio.h>.
 *           Borland Turbo C uses outportb()/inportb() from <dos.h>.
 *           We prefer the Watcom/libi86 names; <dos.h> is NOT included
 *           because its far-pointer typedefs break under -mcmodel=small
 *           with ia16-elf-gcc.
 *
 * delay():  lives in <conio.h> under libi86.  Under DJGPP / Watcom it is
 *           in <dos.h> — if your toolchain puts it there, add <dos.h>
 *           and remove the inline-asm fallback below.
 *
 * Borland target: replace outp/inp with outportb/inportb and add
 *           #include <dos.h> (Borland's dos.h does not use far ptrs).
 * ----------------------------------------------------------------------- */
#  include <conio.h>

/* delay() is in <conio.h> under libi86.  Provide a BIOS-tick fallback
 * for toolchains that lack it (e.g. bare newlib without libi86).        */
#  if !defined(__LIBI86_COMPILING__) && !defined(delay)
static void delay(ubint ms)
{
    /* INT 1Ah AH=00h: read BIOS tick counter (18.2 ticks/sec ~= 1/55ms) */
    unsigned int ticks = ms / 55u + 1u;
    unsigned int start_hi, start_lo, now_hi, now_lo;
    __asm__ volatile (
        "int $0x1a"
        : "=c"(start_hi), "=d"(start_lo)
        : "a"((unsigned int)0x0000)
        : "cc"
    );
    for (;;) {
        __asm__ volatile (
            "int $0x1a"
            : "=c"(now_hi), "=d"(now_lo)
            : "a"((unsigned int)0x0000)
            : "cc"
        );
        /* compare low word only - wraps ~every 24h, fine for short delays */
        if ((unsigned int)(now_lo - start_lo) >= ticks) break;
    }
}
#  endif

static void do_beep(ubint freq, ubint ms)
{
    ubint divisor = (ubint)(1193180UL / freq);
    outp(0x43, 0xB6);
    outp(0x42, (uint8_t)(divisor & 0xFF));
    outp(0x42, (uint8_t)(divisor >> 8));
    outp(0x61, (uint8_t)(inp(0x61) | 0x03));
    delay(ms);
    outp(0x61, (uint8_t)(inp(0x61) & ~0x03));
}
static void  do_delay(ubint ms)          { delay(ms); }
static bint  kbtst(void)                 { return (bint)(kbhit() ? getch() : 0); }
static ubint do_in(ubint p)              { return (ubint)inp(p); }
static void  do_out(ubint p, ubint v)    { outp(p, (uint8_t)v); }

#elif defined(__MINGW32__) || defined(__MINGW64__) || defined(_WIN32)
/* -----------------------------------------------------------------------
 * Windows: MinGW 32/64, MSVC
 * ----------------------------------------------------------------------- */
#  include <windows.h>
#  include <conio.h>

static void  do_beep(ubint freq, ubint ms) { Beep(freq, ms); }
static void  do_delay(ubint ms)            { Sleep(ms); }
static bint  kbtst(void)  { return (bint)(_kbhit() ? _getch() : 0); }
static ubint do_in(ubint p)               { (void)p; return 0; }
static void  do_out(ubint p, ubint v)     { (void)p; (void)v; }

#else
/* -----------------------------------------------------------------------
 * POSIX: Linux / macOS
 * BEEP  -> terminal bell       DELAY -> nanosleep (or clock() fallback)
 * KEY   -> non-blocking termios read
 * INP/OUT -> no-ops (no user-mode port access on protected-mode OS)
 * ----------------------------------------------------------------------- */
#  include <time.h>
#  include <alsa/asoundlib.h>
#  include "tinybeep.h"  // local public domain beeper

static void do_beep(ubint freq, ubint ms)
{
    //(void)freq; (void)ms;
    //fputc('\a', stdout); fflush(stdout);
    snd_lib_error_set_handler(NULL);  /* silence ALSA internal messages */ 

    tinybeep(freq, ms);



}

static void do_delay(ubint ms)
{
#  if defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 199309L
    struct timespec ts;
    ts.tv_sec  = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
#  else
    clock_t end = clock() + (clock_t)(ms * (CLOCKS_PER_SEC / 1000));
    while (clock() < end) ;
#  endif
}

#  include <unistd.h>
#  include <termios.h>
#  include <fcntl.h>

static bint kbtst(void)
{
    struct termios oldt, newt;
    int ch, oldf;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= (tcflag_t)~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);
    ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);
    return (bint)((ch == EOF) ? 0 : ch);
}
static ubint do_in(ubint p)           { (void)p; return 0; }
static void  do_out(ubint p, ubint v) { (void)p; (void)v; }

#endif /* platform HAL */

/* =======================================================================
 * RODATA / RD_BYTE / RD_PTR  — ROM vs RAM address space abstraction
 *
 * On Von Neumann targets (ia16, Z80, 6809, x86) there is one address
 * space; RODATA is just 'const' and the read macros are plain dereferences.
 * The linker places const data in the ROM/flash segment automatically.
 *
 * On Harvard targets (AVR) flash and RAM are separate buses.  String
 * tables must be declared PROGMEM and read back via pgm_read_byte /
 * pgm_read_word — normal pointer dereference will read RAM, not flash.
 *
 * To test the macro layer on a hosted build without real AVR hardware,
 * compile with -DTEST_RODATA.  All macros collapse to normal dereferences
 * so behaviour is identical, but every access goes through the macro path.
 *
 * AVR / Arduino usage:
 *   Compile with -DAVR_PROGMEM (the Arduino toolchain defines __AVR__
 *   automatically; you can also key off that if preferred).
 *
 * RD_BYTE(p)  : read one char/uint8 from a RODATA pointer
 * RD_PTR(pp)  : read one (const char *) from a RODATA pointer-to-pointer
 *               (used to walk reserved_words[] and error_messages[])
 * ======================================================================= */

#if defined(AVR_PROGMEM) || defined(__AVR__)
#  include <avr/pgmspace.h>
#  define RODATA          PROGMEM
#  define RD_BYTE(p)      pgm_read_byte(p)
#  define RD_PTR(pp)      ((const char *)pgm_read_word(pp))
#else
   /* Von Neumann / hosted: plain dereference, const goes to .rodata      */
#  define RODATA          /* nothing */
#  define RD_BYTE(p)      (*(const uint8_t *)(p))
#  define RD_PTR(pp)      (*(pp))
#endif

/* =======================================================================
 * Interpreter constants
 * ======================================================================= */

/* =======================================================================
 * Build-time tuning
 *
 * Define SMALL_TARGET before including / compiling to get a configuration
 * suited to a 64 KB address space (Z80, 6809, AVR, ia16 small model).
 * Individual defines can also be overridden on the compiler command line:
 *   gcc -DBUFFER_SIZE=80 -DNUM_VAR=130 ...
 *
 * BUFFER_SIZE : input line buffer and scratch (bytes)
 * SA_SIZE     : string expression accumulator capacity (bytes)
 *               Must be >= the longest string your program uses.
 * NUM_VAR     : variable slots.  Always (26 * digits_per_letter).
 *               Full set  : 26*10 = 260  (A0..Z9)
 *               Half set  : 26*5  = 130  (A0..Z4)
 *               Minimal   : 26*2  =  52  (A0..Z1, i.e. A,B..Z + one extra)
 * CTL_DEPTH   : control stack depth (FOR + GOSUB frames combined).
 *               Each GOSUB frame = 3 slots, each FOR frame = 6 slots.
 * MAX_FILES   : number of user-accessible file handles (#0 .. #MAX_FILES-1)
 *               CP/M, FLEX, and most small DOSes support 4 open files fine.
 *
 * NOTE(stack): eval_sub() expression depth is capped at 8 levels via the
 *   nest counter; error(13) "Expression too deep" is raised cleanly on
 *   overflow.  Sufficient for all practical integer BASIC programs.
 * ======================================================================= */

#ifdef SMALL_TARGET
#  ifndef BUFFER_SIZE
#    define BUFFER_SIZE   80   /* trim 20 bytes vs default                  */
#  endif
#  ifndef SA_SIZE
#    define SA_SIZE       64   /* strings rarely exceed 64 chars on small targets */
#  endif
#  ifndef NUM_VAR
#    define NUM_VAR      130   /* A0..Z4 : 26*5, halves variable table RAM  */
#  endif
#  ifndef CTL_DEPTH
#    define CTL_DEPTH     24   /* ~4 nested FOR loops or 8 GOSUBs           */
#  endif
#  ifndef MAX_FILES
#    define MAX_FILES      4   /* #0..#3 : enough for CP/M, FLEX, small DOS */
#  endif
#else
#  ifndef BUFFER_SIZE
#    define BUFFER_SIZE  100
#  endif
#  ifndef SA_SIZE
#    define SA_SIZE      100
#  endif
#  ifndef NUM_VAR
#    define NUM_VAR      260   /* A0..Z9 : full variable set                */
#  endif
#  ifndef CTL_DEPTH
#    define CTL_DEPTH     50
#  endif
#  ifndef MAX_FILES
#    define MAX_FILES     10   /* #0..#9                                    */
#  endif
#endif

/* Control stack frame tags - outside bint range so never confused with data */
#define _FOR   1000
#define _GOSUB (_FOR + 1)

/* Primary keyword tokens (1-based; 0 = not found) */
#define LET     1
#define EXIT    2
#define LIST    3
#define NEW     4
#define RUN     5
#define CLEAR   6
#define GOSUB   7
#define GOTO    8
#define RETURN  9
#define PRINT  10
#define FOR    11
#define NEXT   12
#define IF     13
#define LIF    14
#define REM    15
#define STOP   16
#define END    17
#define INPUT  18
#define OPEN   19
#define CLOSE  20
#define DIM    21
#define ORDER  22
#define READ   23
#define DATA   24
#define SAVE   25
#define LOAD   26
#define DELAY  27
#define BEEP   28
#define DOS    29
#define OUT    30

/* Secondary keyword tokens */
#define TO     31   /* lower bound of keyword range used in is_e_end() */
#define STEP   32
#define THEN   33

/* Operator / function tokens */
#define ADD    34   /* lower bound of operator range used in is_e_end() */
#define SUB    35
#define MUL    36
#define DIV    37
#define MOD    38
#define AND    39
#define OR     40
#define XOR    41
#define EQ     42
#define NE     43
#define LE     44
#define SHL    45   /* TODO(bitshift): <<  logical left shift               */
#define LT     46
#define GE     47
#define SHR    48   /* TODO(bitshift): >>  logical right shift              */
#define GT     49
#define CHR    50
#define STR    51
#define ASC    52
#define ABS    53
#define NUM    54
#define RND    55
#define KEY    56
#define INP    57
#define HEX    58   /* HEX$(n) - format bint as uppercase hex string        */
#define UNS    59   /* UNS$(n) - format bint as unsigned decimal string     */
#define UGT    60   /* UGT(a,b) - unsigned greater-than, returns 1/0        */
#define ULT    61   /* ULT(a,b) - unsigned less-than, returns 1/0           */

/* Pseudo-command: RUN without clearing variables (used by LOAD-in-program) */
#define RUN1   99

/* Operator priority table, indexed from 0 by (op_token - (ADD-1)).
 * Index = token - 33.
 *  1- 8: ADD SUB MUL DIV MOD AND OR  XOR   (arithmetic + bitwise)
 *  9-11: EQ  NE  LE                        (equality, <=)
 * 12   : SHL                               (<< priority 3 = same as & | ^)
 * 13   : LT                                (<)
 * 14   : GE                                (>=)
 * 15   : SHR                               (>> priority 3 = same as & | ^)
 * 16   : GT                                (>)
 * TODO(bitshift): SHL/SHR slots at 12 and 15. */
static const uint8_t RODATA priority[] = {
    0,                /* 0  sentinel                                        */
    1, 1, 2, 2, 2,    /* 1- 5: ADD SUB MUL DIV MOD                         */
    3, 3, 3,          /* 6- 8: AND OR  XOR                                 */
    1, 1, 1,          /* 9-11: EQ  NE  LE                                  */
    3,                /* 12  : SHL                                         */
    1,                /* 13  : LT                                          */
    1,                /* 14  : GE                                          */
    3,                /* 15  : SHR                                         */
    1                 /* 16  : GT                                          */
};

/* Reserved word strings - order must match token #defines above */
static const char * const RODATA reserved_words[] = {
    "LET",   "EXIT",  "LIST",  "NEW",   "RUN",   "CLEAR", "GOSUB", "GOTO",
    "RETURN","PRINT", "FOR",   "NEXT",  "IF",    "LIF",   "REM",   "STOP",
    "END",   "INPUT", "OPEN",  "CLOSE", "DIM",   "ORDER", "READ",  "DATA",
    "SAVE",  "LOAD",  "DELAY", "BEEP",  "DOS",   "OUT",
    "TO",    "STEP",  "THEN",
    "+", "-", "*", "/", "%", "&", "|", "^",
    "=", "<>", "<=", "<<", "<", ">=", ">>", ">",
    "CHR$(", "STR$(", "ASC(", "ABS(", "NUM(", "RND(", "KEY(", "INP(",
    "HEX$(", "UNS$(",
    "UGT(",  "ULT(",
    NULL
};

/* Error messages, indexed by error number */
static const char * const RODATA error_messages[] = {
    "Syntax",            /*  0 */
    "Illegal program",   /*  1 */
    "Illegal direct",    /*  2 */
    "Line number",       /*  3 */
    "Wrong type",        /*  4 */
    "Divide by zero",    /*  5 */
    "Nesting",           /*  6 */
    "File not open",     /*  7 */
    "File already open", /*  8 */
    "Input",             /*  9 */
    "Dimension",         /* 10 */
    "Data",              /* 11 */
    "Out of memory",     /* 12 */
    "Expression too deep"/* 13 */
};

/* =======================================================================
 * Program line storage
 * Lines are kept in a singly-linked list sorted by line number.
 * Ltext[] is a flexible member allocated with extra bytes for the
 * tokenised line content.
 * ======================================================================= */
struct line_rec {
    ubint            Lnumber;
    struct line_rec *Llink;
    char             Ltext[1];
};

/* =======================================================================
 * Global interpreter state
 * ======================================================================= */

static char sa1[SA_SIZE], sa2[SA_SIZE]; /* string expression accumulators  */

static struct line_rec *pgm_start;  /* head of program line list            */
static struct line_rec *pgm_end;    /* tail of program line list (last line) */
static struct line_rec *runptr;     /* line currently being executed        */
static struct line_rec *readptr;    /* current DATA line for READ           */

static bint   num_vars[NUM_VAR];    /* numeric (integer) variables          */
static bint  *dim_vars[NUM_VAR];    /* dimensioned (array) variable storage */
static char  *char_vars[NUM_VAR];   /* string variable storage              */
static ubint  dim_check[NUM_VAR];   /* allocated sizes of dim arrays        */

static FILE  *files[MAX_FILES];     /* user-accessible file handles 0..MAX_FILES-1 */
static FILE  *filein, *fileout;     /* active I/O streams for current stmt  */

static char   buffer[BUFFER_SIZE];  /* raw input line / scratch             */
static char  *cmdptr;               /* parse cursor (buffer or Ltext)       */
static char  *dataptr;              /* parse cursor within a DATA line      */
static char   filename[256];        /* current LOAD / SAVE filename         */

static char   mode     = 0;         /* 0 = interactive, nonzero = running   */
static char   expr_type;            /* 0 = numeric result, 1 = string       */
static char   nest;                 /* parenthesis / sub-expression depth   */
static ubint  line;                 /* current line number                  */

/* Control stack.  Entries are either small bint values (step, limit,
 * variable index, frame tag) or data pointers (runptr, cmdptr).
 * bptr is the only type wide enough to hold both on all targets.        */
static ubint  ctl_ptr = 0;
static bptr   ctl_stk[CTL_DEPTH];

static jmp_buf savjmp;              /* error recovery / END / STOP / NEW   */

/* =======================================================================
 * Forward declarations
 * ======================================================================= */
static int           is_e_end(tok_t c);
static int           is_l_end(tok_t c);
static int           isterm(tok_t c);
static tok_t         skip_blank(void);
static tok_t         get_next(void);
static int           test_next(tok_t token);
static void          expect(tok_t token);
static ubint         lookup(const char * const RODATA table[]);
static ubint         get_num(void);
static char         *allocate(ubint size);
static void          delete_line(ubint lino);
static void          insert_line(ubint lino);
static int           edit_program(void);
static struct line_rec *find_line(ubint lno);
static bint         *num_address(void);
static char        **str_address(void);
static struct line_rec *execute(tok_t cmd);
static int           chk_file(int flag);
static void          disp_pgm(FILE *fp, ubint i, ubint j);
static void          pgm_only(void);
static void          direct_only(void);
static void          skip_stmt(void);
static void          error(ubint en);
static bint          eval_num(void);
static void          eval_char(void);
static bint          eval(void);
static bint          eval_sub(void);
static bint          get_value(void);
static void          get_char_value(char *ptr);
static bint          do_arith(int opr, bint op1, bint op2);
static void          num_string(bint value, char *ptr);
static void          clear_pgm(void);
static void          clear_vars(void);
static ubint         get_var(void);
static void          concat(char *dst, const char *a, const char *b);

/* =======================================================================
 * concat() - replaces the Micro-C library built-in
 * ======================================================================= */
static void concat(char *dst, const char *a, const char *b)
{
    while (*a) *dst++ = *a++;
    while (*b) *dst++ = *b++;
    *dst = '\0';
}

/* =======================================================================
 * Token / character classification helpers
 * ======================================================================= */

/* True at the end of an expression token stream */
static int is_e_end(tok_t c)
{
    if (c >= TOKEN(TO) && c < TOKEN(ADD)) return 1;
    return (c == '\0') || (c == ':') || (c == ')') || (c == ',') || (c == ';');
}

/* True at the end of a statement */
static int is_l_end(tok_t c)
{
    return (c == '\0') || (c == ':');
}

/* True for horizontal whitespace */
static int isterm(tok_t c)
{
    return (c == ' ') || (c == '\t');
}

/* Advance past whitespace; return next byte without consuming it */
static tok_t skip_blank(void)
{
    while (isterm((tok_t)*cmdptr)) ++cmdptr;
    return (tok_t)*cmdptr;
}

/* Advance past whitespace, consume and return the next byte */
static tok_t get_next(void)
{
    tok_t c;
    while (isterm((tok_t)(c = (tok_t)*cmdptr))) ++cmdptr;
    if (c) ++cmdptr;
    return c;
}

/* If the next non-blank byte equals token, consume it and return true */
static int test_next(tok_t token)
{
    if (skip_blank() == token) { ++cmdptr; return 1; }
    return 0;
}

/* Consume the next non-blank byte; syntax error if it != token */
static void expect(tok_t token)
{
    if (get_next() != token) error(0);
}

/* =======================================================================
 * lookup() - match next token in cmdptr against reserved_words[]
 * Returns 1-based index on match (advancing cmdptr past it), 0 otherwise.
 * RD_PTR/RD_BYTE used so the table can live in AVR flash (PROGMEM).
 * ======================================================================= */
static ubint lookup(const char * const RODATA table[])
{
    ubint       i;
    const char *cptr;
    char       *optr = cmdptr;

    for (i = 0; (cptr = RD_PTR(&table[i])) != NULL; ++i) {
        while (RD_BYTE(cptr) &&
               (RD_BYTE(cptr) == toupper((unsigned char)*cmdptr))) {
            ++cptr; ++cmdptr; }
        if (!RD_BYTE(cptr)) {
            /* Avoid matching "FOR" inside "FORMAT": reject if both the
             * last-matched char and the very next input char are alnum. */
            if (!(isalnum((unsigned char)RD_BYTE(cptr-1)) &&
                  isalnum((unsigned char)*cmdptr))) {
                skip_blank();
                return (ubint)(i + 1); } }
        cmdptr = optr; }
    return 0;
}

/* =======================================================================
 * get_num() - parse a numeric literal from cmdptr.
 *
 * TODO(literals): Dunfield-style prefix notation.
 *   #xxxx  hexadecimal   e.g. #FF, #1A2B
 *   @dddd  unsigned dec  e.g. @65535, @32768
 *   dddd   signed dec    unchanged (no prefix)
 *
 * All paths return ubint; the caller in get_value() casts to bint.
 * Storage stays int16_t throughout -- overflow calls error(0).
 * Invalid digits for the active base also call error(0).
 *
 * Plain decimal (no prefix) is unchanged -- callers that parse line
 * numbers, port numbers, etc. only ever see digits, never a prefix,
 * so they are unaffected.
 * ======================================================================= */
static ubint get_num(void)
{
    ubint value = 0;
    char  c;

    c = *cmdptr;

    if (c == '#') {                         /* --- hexadecimal --- */
        ++cmdptr;
        if (!isxdigit((unsigned char)*cmdptr)) error(0);
        while (isxdigit((unsigned char)(c = *cmdptr))) {
            ubint digit;
            ++cmdptr;
            if      (c >= '0' && c <= '9') digit = (ubint)(c - '0');
            else if (c >= 'a' && c <= 'f') digit = (ubint)(c - 'a' + 10);
            else                           digit = (ubint)(c - 'A' + 10);
            if (value > (ubint)0x0FFF) error(0);   /* would overflow ubint */
            value = (ubint)((value << 4) | digit); }

    } else if (c == '@') {                  /* --- unsigned decimal --- */
        ubint tmp;
        ++cmdptr;
        if (!isdigit((unsigned char)*cmdptr)) error(0);
        while (isdigit((unsigned char)(c = *cmdptr))) {
            ++cmdptr;
            tmp = (ubint)(value * 10 + (ubint)(c - '0'));
            if (tmp < value) error(0);      /* wrapped: value > 65535        */
            value = tmp; }

    } else {                                /* --- plain signed decimal --- */
        while (isdigit((unsigned char)(c = *cmdptr))) {
            ++cmdptr;
            value = (ubint)(value * 10 + (ubint)(c - '0')); }
    }

    return value;
}

/* =======================================================================
 * allocate() - malloc + zero; calls error(12) on failure
 * ======================================================================= */
static char *allocate(ubint size)
{
    char *ptr = (char *)calloc(1, size);
    if (!ptr) error(12);
    return ptr;
}

/* =======================================================================
 * Program line list management
 * ======================================================================= */

static void delete_line(ubint lino)
{
    struct line_rec *cur, *prev = NULL;
    for (cur = pgm_start; cur; prev = cur, cur = cur->Llink) {
        if (cur->Lnumber == lino) {
            if (prev) prev->Llink = cur->Llink;
            else      pgm_start   = cur->Llink;
            free(cur);
            /* if we deleted the tail, rescan for new tail */
            if (pgm_end == cur) {
                pgm_end = NULL;
                for (cur = pgm_start; cur; cur = cur->Llink)
                    pgm_end = cur; }
            return; } }
}

static void insert_line(ubint lino)
{
    struct line_rec *node, *cur, *prev = NULL;
    char            *src = cmdptr;
    ubint            len;

    for (len = (ubint)sizeof(struct line_rec); *src; ++len, ++src) ;
    node = (struct line_rec *)allocate((ubint)(len + 1));
    node->Lnumber = lino;
    for (len = 0; *cmdptr; ++len) node->Ltext[len] = *cmdptr++;
    node->Ltext[len] = '\0';

    for (cur = pgm_start; cur && cur->Lnumber < lino;
         prev = cur, cur = cur->Llink) ;
    node->Llink = cur;
    if (prev) prev->Llink = node;
    else      pgm_start   = node;

    /* if node has no successor it is the new tail */
    if (!node->Llink) pgm_end = node;
}

/* =======================================================================
 * edit_program()
 * Tokenises buffer[], then inserts or removes the line.
 * Returns non-zero when the input was a numbered source line.
 * ======================================================================= */
static int edit_program(void)
{
    ubint  value;
    char  *ptr;
    tok_t  c;

    /* Strip trailing CR/LF - fgets() keeps them; original Micro-C did not */
    { char *nl = buffer + strlen(buffer);
      while (nl > buffer && (*(nl-1) == '\n' || *(nl-1) == '\r')) *--nl = '\0'; }

    /* Tokenise: replace reserved words with (index | 0x80) bytes */
    cmdptr = ptr = buffer;
    while ((c = (tok_t)*cmdptr) != 0) {
        if ((value = lookup(reserved_words)) != 0) {
            *ptr++ = (char)(value | 0x80);
        } else {
            *ptr++ = (char)c; ++cmdptr;
            if (c == '"') {             /* pass string literals verbatim    */
                while ((c = (tok_t)*cmdptr) && c != '"') { ++cmdptr; *ptr++ = (char)c; }
                *ptr++ = *cmdptr++; } } }
    *ptr   = '\0';
    cmdptr = buffer;

    if (isdigit((unsigned char)skip_blank())) {
        value = get_num();
        delete_line(value);
        if (skip_blank()) insert_line(value);
        return 1; }
    return 0;
}

/* =======================================================================
 * find_line() - locate line by number; error(3) if not found
 * ======================================================================= */
static struct line_rec *find_line(ubint lno)
{
    struct line_rec *p;
    for (p = pgm_start; p; p = p->Llink)
        if (p->Lnumber == lno) return p;
    error(3);
    return NULL;    /* unreachable - error() longjmps */
}

/* =======================================================================
 * Lvalue address helpers
 *
 * Two typed helpers replace the previous uintptr_t* trick, giving the
 * compiler real type information and eliminating pointer-width casts.
 *
 * Both advance cmdptr past the variable name (and subscript).
 * expr_type is set as a side effect of get_var() inside each helper.
 * ======================================================================= */

/* Return pointer to the numeric lvalue named at cmdptr */
static bint *num_address(void)
{
    ubint idx = get_var();
    ubint sub;
    if (expr_type) error(4);            /* string var where numeric expected */
    if (test_next('(')) {               /* array element */
        bint *arr = dim_vars[idx];
        if (!arr) error(10);
        nest = 0;
        sub  = (ubint)eval_sub();
        if (sub >= dim_check[idx]) error(10);
        return &arr[sub]; }
    return &num_vars[idx];
}

/* Return pointer to the string lvalue named at cmdptr */
static char **str_address(void)
{
    ubint idx = get_var();
    if (!expr_type) error(4);           /* numeric var where string expected */
    return &char_vars[idx];
}

/* =======================================================================
 * execute() - dispatch one BASIC statement
 * Returns pointer to the next line_rec (for GOTO/GOSUB/IF),
 * or NULL to continue with the next statement on the current line.
 * ======================================================================= */
static struct line_rec *execute(tok_t cmd)
{
    ubint           i, j;
    bint            ii, jj, val;
    struct line_rec *lp;
    tok_t           c;

    switch ((int)(cmd & 0x7F)) {

    /* ---- LET : variable = expression ---------------------------------- */
    case LET : {
        /* Peek at variable type before touching cmdptr permanently */
        char  *save  = cmdptr;
        ubint  vtype;
        get_var();
        vtype  = (ubint)expr_type;
        cmdptr = save;

        if (vtype) {                        /* string assignment             */
            char **dp = str_address();
            expect(TOKEN(EQ));
            eval_char();                    /* result lands in sa1           */
            if (*dp) free(*dp);
            *dp = *sa1 ? strcpy(allocate((ubint)(strlen(sa1)+1)), sa1) : NULL;
        } else {                            /* numeric assignment            */
            bint *dp = num_address();
            expect(TOKEN(EQ));
            *dp = eval();
        }
        break; }

    /* ---- EXIT ---------------------------------------------------------- */
    case EXIT :
        exit(0);

    /* ---- LIST [start[,end]] ------------------------------------------- */
    case LIST :
        chk_file(1);
        if (!isdigit((unsigned char)skip_blank())) {
            i = 0; j = (ubint)-1;
        } else {
            i = get_num();
            if (get_next() == ',')
                j = isdigit((unsigned char)skip_blank()) ? get_num() : (ubint)-1;
            else
                j = i; }
        disp_pgm(fileout, i, j);
        break;

    /* ---- NEW ----------------------------------------------------------- */
    case NEW :
        clear_vars(); clear_pgm(); longjmp(savjmp, 1);

    /* ---- RUN [line] ---------------------------------------------------- */
    case RUN :
        direct_only(); clear_vars();
        /* fall through */

    /* ---- RUN1 (no clear - used by LOAD mid-program) ------------------- */
    case RUN1 :
        runptr = is_e_end(skip_blank()) ? pgm_start
                                        : find_line((ubint)eval_num());
        --mode;
newline:
        while (runptr) {
            cmdptr = runptr->Ltext;
            line   = runptr->Lnumber;
            do {
                if ((c = skip_blank()) < 0) {
                    ++cmdptr;
                    if ((lp = execute(c)) != NULL) { runptr = lp; goto newline; }
                } else {
                    execute((tok_t)LET); }
            } while ((c = get_next()) == ':');
            if (c) error(0);
            runptr = runptr->Llink; }
        mode = 0;
        break;

    /* ---- CLEAR --------------------------------------------------------- */
    case CLEAR :
        clear_vars(); break;

    /* ---- GOSUB line ---------------------------------------------------- */
    case GOSUB :
        ctl_stk[ctl_ptr++] = (bptr)runptr;
        ctl_stk[ctl_ptr++] = (bptr)cmdptr;
        ctl_stk[ctl_ptr++] = (bptr)_GOSUB;
        /* fall through */

    /* ---- GOTO line ----------------------------------------------------- */
    case GOTO :
        pgm_only();
        return find_line((ubint)eval_num());

    /* ---- RETURN -------------------------------------------------------- */
    case RETURN :
        pgm_only();
        if ((int)ctl_stk[--ctl_ptr] != _GOSUB) error(6);
        cmdptr = (char       *)ctl_stk[--ctl_ptr];
        runptr = (struct line_rec *)ctl_stk[--ctl_ptr];
        line   = runptr->Lnumber;
        skip_stmt();
        break;

    /* ---- PRINT --------------------------------------------------------- */
    case PRINT : {
        /* delim tracks the separator just consumed:
         *   0 = start / after comma  (normal spacing, newline at end)
         *   1 = after semicolon      (no space before next item)
         * A trailing , or ; suppresses the final newline.            */
        int delim = 0;
        int suppress_nl = 0;
        chk_file(1);
        while (!is_l_end(skip_blank())) {
            val = eval();
            if (!expr_type) { num_string(val, sa1); if (!delim) putc(' ', fileout); }
            fputs(sa1, fileout);
            if      (test_next(';')) { delim = 1; suppress_nl = 1; }
            else if (test_next(',')) { delim = 0; suppress_nl = 1; }
            else                    { suppress_nl = 0; break; }
        }
        if (!suppress_nl) putc('\n', fileout);
        break; }

    /* ---- FOR v = init TO limit [STEP n] ------------------------------- */
    case FOR :
        pgm_only();
        ii = 1;
        i  = get_var(); if (expr_type) error(0);
        expect(TOKEN(EQ));
        num_vars[i] = eval(); if (expr_type) error(0);
        expect(TOKEN(TO));
        jj = eval();
        if (test_next(TOKEN(STEP))) ii = eval();
        skip_stmt();
        ctl_stk[ctl_ptr++] = (bptr)runptr;  /* saved line ptr  */
        ctl_stk[ctl_ptr++] = (bptr)cmdptr;  /* saved cmd ptr   */
        ctl_stk[ctl_ptr++] = (bptr)ii;      /* step            */
        ctl_stk[ctl_ptr++] = (bptr)jj;      /* limit           */
        ctl_stk[ctl_ptr++] = (bptr)i;       /* variable index  */
        ctl_stk[ctl_ptr++] = (bptr)_FOR;
        break;

    /* ---- NEXT [v] ------------------------------------------------------ */
    case NEXT :
        pgm_only();
        if ((int)ctl_stk[ctl_ptr-1] != _FOR) error(6);
        i  = (ubint)              ctl_stk[ctl_ptr-2];
        if (!is_l_end(skip_blank()))
            if (get_var() != i) error(6);
        jj = (bint)(intptr_t)ctl_stk[ctl_ptr-3];
        ii = (bint)(intptr_t)ctl_stk[ctl_ptr-4];
        num_vars[i] = (bint)(num_vars[i] + ii);
        if ((ii < 0) ? (num_vars[i] >= jj) : (num_vars[i] <= jj)) {
            cmdptr = (char       *)ctl_stk[ctl_ptr-5];
            runptr = (struct line_rec *)ctl_stk[ctl_ptr-6];
            line   = runptr->Lnumber;
        } else { ctl_ptr -= 6; }
        break;

    /* ---- IF test THEN line | stmt ------------------------------------- */
    case IF :
        val = eval_num(); expect(TOKEN(THEN));
        if (val) {
            c = skip_blank();
            if (isdigit((unsigned char)c)) return find_line((ubint)eval_num());
            if (c < 0) { ++cmdptr; return execute(c); }
            execute((tok_t)LET);
        } else { skip_stmt(); }
        break;

    /* ---- LIF test THEN stmts ------------------------------------------ */
    case LIF :
        val = eval_num(); expect(TOKEN(THEN));
        if (val) {
            c = skip_blank();
            if (c < 0) { ++cmdptr; return execute(c); }
            execute((tok_t)LET);
            break; }
        /* condition false: fall through to DATA/REM skip behaviour */
        /* fall through */

    /* ---- DATA / REM : skip to next line in running mode --------------- */
    case DATA :
        pgm_only();
        /* fall through */
    case REM :
        if (mode) {
            if ((lp = runptr->Llink) != NULL) return lp;
            longjmp(savjmp, 1); }
        break;

    /* ---- STOP ---------------------------------------------------------- */
    case STOP :
        pgm_only();
        printf("STOP in line %u\n", (unsigned)line);
        /* fall through */

    /* ---- END ----------------------------------------------------------- */
    case END :
        pgm_only(); longjmp(savjmp, 1);

    /* ---- INPUT ["prompt",] var ---------------------------------------- */
    case INPUT : {
        int   from_file = chk_file(1);
        char *save_cmd;
        ubint vtype;

        /* Evaluate optional prompt string into sa1 */
        if (skip_blank() == '"') { eval_char(); expect(','); }
        else strcpy(sa1, "? ");

        /* Peek at variable type without advancing cmdptr permanently */
        { char *sp = cmdptr; get_var(); vtype = (ubint)expr_type; cmdptr = sp; }
        save_cmd = cmdptr;

        for (;;) {                          /* retry loop for bad numeric input */
            if (from_file == -1) fputs(sa1, stdout);
            { char *r = fgets(buffer, (int)(sizeof(buffer)-1), filein);
              if (!r) buffer[0] = '\0'; }     /* EOF or error -> empty buffer */

            if (vtype) {                    /* string input */
                char **dp;
                { char *nl = buffer+strlen(buffer);
                  while (nl>buffer&&(*(nl-1)=='\n'||*(nl-1)=='\r')) *--nl='\0'; }
                cmdptr = save_cmd;
                dp = str_address();
                if (*dp) free(*dp);
                *dp = *buffer ? strcpy(allocate((ubint)(strlen(buffer)+1)),buffer) : NULL;
                break;
            } else {                        /* numeric input */
                bint  neg = 0;
                bint *dp;
                cmdptr = buffer;
                if (test_next(TOKEN(SUB))) neg = 1;
                if (!isdigit((unsigned char)*cmdptr)) {
                    if (from_file != -1) error(9);
                    fputs("Input error\n", stdout);
                    continue; }             /* retry */
                j      = get_num();
                cmdptr = save_cmd;
                dp     = num_address();
                *dp    = neg ? (bint)(0-(bint)j) : (bint)j;
                break; } }

        /* cmdptr is now correctly positioned after the variable name */
        break; }

    /* ---- OPEN#n,"name","mode" ----------------------------------------- */
    case OPEN :
        if (skip_blank() != '#') error(0);
        i = (ubint)chk_file(0);
        if (files[i]) error(8);
        eval_char(); strcpy(buffer, sa1);
        expect(',');
        eval_char();
        files[i] = fopen(buffer, sa1);
        break;

    /* ---- CLOSE#n ------------------------------------------------------- */
    case CLOSE :
        i = (ubint)chk_file(1);
        if (!filein) error(8);
        fclose(files[i]); files[i] = NULL;
        break;

    /* ---- DIM var(size)[,...] ------------------------------------------ */
    case DIM :
        do {
            i = get_var(); if (expr_type) error(0);
            if (dim_vars[i]) free(dim_vars[i]);
            dim_check[i] = (ubint)(eval_num() + 1);
            dim_vars[i]  = (bint *)allocate((ubint)(dim_check[i] * sizeof(bint)));
        } while (test_next(','));
        break;

    /* ---- ORDER line ---------------------------------------------------- */
    case ORDER : {
        char *save;
        readptr = find_line((ubint)eval_num());
        save    = cmdptr;               /* save position AFTER parsing line number */
        cmdptr  = readptr->Ltext;
        if (get_next() != TOKEN(DATA)) error(11);
        dataptr = cmdptr;
        cmdptr  = save;
        break; }

    /* ---- READ var[,...] ----------------------------------------------- */
    case READ :
        do {
            char  *save_cmd  = cmdptr;
            ubint  save_line = line;
            ubint  vtype;

            { char *sp = cmdptr; get_var(); vtype = (ubint)expr_type; cmdptr = sp; }

            cmdptr = dataptr;
            if (!skip_blank()) {
                readptr = readptr->Llink;
                cmdptr  = readptr->Ltext;
                if (get_next() != TOKEN(DATA)) error(11); }
            line = readptr->Lnumber;

            if (vtype) {                    /* string READ */
                char **dp;
                eval_char();
                test_next(',');
                dataptr = cmdptr; cmdptr = save_cmd; line = save_line;
                dp = str_address();
                if (*dp) free(*dp);
                *dp = *sa1 ? strcpy(allocate((ubint)(strlen(sa1)+1)), sa1) : NULL;
            } else {                        /* numeric READ */
                bint  rv = eval();
                bint *dp;
                if (expr_type) error(11);
                test_next(',');
                dataptr = cmdptr; cmdptr = save_cmd; line = save_line;
                dp  = num_address();
                *dp = rv; }
        } while (test_next(','));
        break;

    /* ---- DELAY ms ------------------------------------------------------ */
    case DELAY :
        do_delay((ubint)eval_num()); break;

    /* ---- BEEP freq,ms -------------------------------------------------- */
    case BEEP :
        i = (ubint)eval_num(); expect(',');
        do_beep(i, (ubint)eval_num()); break;

    /* ---- DOS "command" ------------------------------------------------- */
    case DOS :
        eval_char();
#if defined(__ia16__) || defined(__MSDOS__) || defined(_MSDOS)
        /* newlib for ia16 does not provide system(); use INT 21h AH=4Bh
         * (EXEC) via libi86's _dos_system() if available, otherwise no-op */
#   if defined(_DOS_SYSTEM_DEFINED)
        { int r = _dos_system(sa1); (void)r; }
#   else
        (void)sa1;
#   endif
#else
        { int r = system(sa1); (void)r; }
#endif
        break;

    /* ---- OUT port,val -------------------------------------------------- */
    case OUT :
        i = (ubint)eval_num(); expect(',');
        do_out(i, (ubint)eval_num()); break;

    /* ---- SAVE ["name"] ------------------------------------------------- */
    case SAVE :
        direct_only();
        if (skip_blank()) { eval_char(); concat(filename, sa1, ".BAS"); }
        { FILE *fp = fopen(filename, "wb");
          if (fp) { disp_pgm(fp, 0, (ubint)-1); fclose(fp); } }
        break;

    /* ---- LOAD "name" --------------------------------------------------- */
    case LOAD :
        eval_char(); concat(filename, sa1, ".BAS");
        { FILE *fp = fopen(filename, "rb");
          if (fp) {
              if (!mode) clear_vars();
              clear_pgm();
              while (fgets(buffer, (int)(sizeof(buffer)-1), fp)) edit_program();
              fclose(fp);
              return pgm_start; } }
        longjmp(savjmp, 1);

    default : error(0); }

    return NULL;
}

/* =======================================================================
 * chk_file() - parse optional #n file specifier; set filein/fileout.
 * Returns file index 0-9 or -1 for console.
 * flag != 0: error(7) if the file handle is not currently open.
 * ======================================================================= */
static int chk_file(int flag)
{
    int i = -1;
    if (test_next('#')) {
        i = (int)(bint)eval_num();
        if (i < 0 || i >= MAX_FILES) error(7);
        test_next(',');
        filein = fileout = files[i];
        if (flag && !filein) error(7);
    } else { filein = stdin; fileout = stdout; }
    return i;
}

/* =======================================================================
 * disp_pgm() - list tokenised source lines to fp, lines i..j inclusive
 * ======================================================================= */
static void disp_pgm(FILE *fp, ubint i, ubint j)
{
    struct line_rec *p;
    tok_t            c;
    ubint            k;

    for (p = pgm_start; p; p = p->Llink) {
        k = p->Lnumber;
        if (k >= i && k <= j) {
            fprintf(fp, "%u ", (unsigned)k);
            for (k = 0; (c = (tok_t)p->Ltext[k]) != 0; ++k) {
                if (c < 0) {
                    int         idx  = (c & 0x7F) - 1;
                    const char *wptr = RD_PTR(&reserved_words[idx]);
                    uint8_t     ch;
                    while ((ch = RD_BYTE(wptr)) != 0) { putc((char)ch, fp); ++wptr; }
                    if ((c & 0x7F) < ADD) putc(' ', fp);
                } else { putc((char)c, fp); } }
            putc('\n', fp); } }
}

/* =======================================================================
 * Mode guards
 * ======================================================================= */
static void pgm_only(void)    { if (!mode) error(2); }
static void direct_only(void) { if (mode)  error(1); }

/* =======================================================================
 * skip_stmt() - advance cmdptr to end of current statement (: or NUL)
 * ======================================================================= */
static void skip_stmt(void)
{
    char c;
    while ((c = *cmdptr) && c != ':') {
        ++cmdptr;
        if (c == '"') {
            while ((c = *cmdptr) && c != '"') ++cmdptr;
            if (c) ++cmdptr; } }
}

/* =======================================================================
 * error() - print message and longjmp back to the main loop
 * ======================================================================= */
static void error(ubint en)
{
    printf("%s error", RD_PTR(&error_messages[en]));
    if (mode) printf(" in line %u", (unsigned)line);
    putc('\n', stdout);
    longjmp(savjmp, 1);
}

/* =======================================================================
 * Expression evaluators
 * ======================================================================= */

/* Evaluate; require numeric result (error(4) on string) */
static bint eval_num(void)
{
    bint v = eval();
    if (expr_type) error(4);
    return v;
}

/* Evaluate; require string result (error(4) on numeric); result in sa1 */
static void eval_char(void)
{
    eval();
    if (!expr_type) error(4);
}

/* Top-level: evaluate a full expression, reset nest counter */
static bint eval(void)
{
    bint v;
    nest = 0;
    v    = eval_sub();
    if (nest != 1) error(0);
    return v;
}

/*
 * eval_sub() - precedence-climbing expression evaluator.
 *
 * Two small stacks (operand bint[], operator int[]) handle precedence
 * without building a parse tree.  expr_type is 0 (numeric) or 1 (string)
 * on exit.
 */
static bint eval_sub(void)
{
    bint  nstack[8];
    int   ostack[8];
    ubint nptr, optr;
    tok_t c;

    if (++nest > 8) error(13);
    ostack[optr = nptr = 0] = 0;        /* sentinel */
    nstack[++nptr] = get_value();

    if (expr_type) {
        /* String expression: only + (concat) and =/< > (compare) valid */
        while (!is_e_end(c = skip_blank())) {
            int op = c & 0x7F;
            ++cmdptr;
            get_char_value(sa2);
            if      (op == ADD) { strcat(sa1, sa2); }
            else if (op == EQ)  { nstack[nptr] = (bint)(!strcmp(sa1,sa2)); expr_type=0; }
            else if (op == NE)  { nstack[nptr] = (bint)(strcmp(sa1,sa2)!=0); expr_type=0; }
            else                { error(0); } }
    } else {
        /* Numeric expression: full operator set with precedence */
        while (!is_e_end(c = skip_blank())) {
            int   op  = (c & 0x7F) - (ADD - 1);
            bint  rhs;
            ++cmdptr;
            if ((ubint)priority[op] <= (ubint)priority[ostack[optr]]) {
                rhs          = nstack[nptr--];
                nstack[nptr] = do_arith(ostack[optr--], nstack[nptr], rhs); }
            nstack[++nptr] = get_value();
            if (expr_type) error(0);
            ostack[++optr] = op; }
        while (optr) {
            bint rhs     = nstack[nptr--];
            nstack[nptr] = do_arith(ostack[optr--], nstack[nptr], rhs); } }

    if (c == ')') { --nest; ++cmdptr; }
    return nstack[nptr];
}

/*
 * get_value() - parse one value: literal, variable, function, unary op,
 * or parenthesised sub-expression.  Sets expr_type.
 */
static bint get_value(void)
{
    bint  value = 0;
    tok_t c     = skip_blank();

    if (isdigit((unsigned char)c) || c == '#' || c == '@') {
        /* TODO(literals): plain decimal OR prefixed literal (#hex, @udec).
         * get_num() reads the prefix itself from *cmdptr.                  */
        expr_type = 0;
        value     = (bint)get_num();
    } else {
        ++cmdptr;
        expr_type = 0;                  /* default; overridden below        */
        switch ((int)c) {

        case '(' :
            value = eval_sub(); break;

        case '!' :                      /* unary bitwise NOT                */
            value = (bint)~(ubint)get_value(); break;

        case TOKEN(SUB) :               /* unary minus                      */
            value = (bint)-(ubint)get_value(); break;

        case TOKEN(ASC) :               /* ASC(s) -> char code              */
            eval_sub(); if (!expr_type) error(4);
            value = (bint)(unsigned char)sa1[0]; expr_type = 0; break;

        case TOKEN(NUM) :               /* NUM(s) -> integer                */
            eval_sub(); if (!expr_type) error(4);
            value = (bint)atoi(sa1); expr_type = 0; break;

        case TOKEN(ABS) :               /* ABS(n)                           */
            value = eval_sub();
            if (value < 0) value = (bint)-value;
            goto number_only;

        case TOKEN(RND) : {             /* RND(n) -> 0..n-1                 */
            ubint range = (ubint)eval_sub();
            value = range ? (bint)(rand() % (int)range) : 0;
            goto number_only; }

        case TOKEN(KEY) :               /* KEY() -> keycode or 0            */
            expect(')'); value = kbtst(); break;

        case TOKEN(INP) :               /* INP(port)                        */
            value = (bint)do_in((ubint)eval_sub());
            goto number_only;

        case TOKEN(UGT) : {             /* UGT(a,b) -> unsigned a > b       */
            ubint a = (ubint)eval_sub();
            if (expr_type) error(4);
            --nest;                     /* comma closes first arg context   */
            expect(',');
            ubint b = (ubint)eval_sub();
            value = (bint)(a > b);
            goto number_only; }

        case TOKEN(ULT) : {             /* ULT(a,b) -> unsigned a < b       */
            ubint a = (ubint)eval_sub();
            if (expr_type) error(4);
            --nest;                     /* comma closes first arg context   */
            expect(',');
            ubint b = (ubint)eval_sub();
            value = (bint)(a < b); }
number_only:
            if (expr_type) error(4);
            break;

        default :
            --cmdptr;
            if (isalpha((unsigned char)c)) {
                ubint idx = get_var();
                if (expr_type) {            /* string variable              */
                    const char *p = char_vars[idx];
                    strcpy(sa1, p ? p : "");
                } else if (test_next('(')) {/* array element                */
                    bint *arr = dim_vars[idx]; ubint sub;
                    if (!arr) error(10);
                    sub = (ubint)eval_sub();
                    if (sub >= dim_check[idx]) error(10);
                    value = arr[sub];
                } else {                    /* scalar numeric               */
                    value = num_vars[idx]; }
            } else {
                get_char_value(sa1); }      /* string literal / CHR$ / STR$ */
            break; } }

    return value;
}

/*
 * get_char_value() - parse a string value into *ptr.
 * Accepts: string literal, string variable, CHR$(n), STR$(n).
 * Sets expr_type = 1 on exit.
 */
static void get_char_value(char *ptr)
{
    tok_t c = get_next();

    if (c == '"') {                     /* string literal                   */
        while ((*ptr = *cmdptr++) != '"') { if (!*ptr) error(0); ++ptr; }
        *ptr = '\0';
    } else if (isalpha((unsigned char)c)) { /* string variable             */
        --cmdptr;
        { ubint idx = get_var();
          const char *p;
          if (!expr_type) error(0);
          p = char_vars[idx];
          strcpy(ptr, p ? p : ""); }
    } else if (c == TOKEN(CHR)) {       /* CHR$(n)                          */
        *ptr++ = (char)(ubint)eval_sub();
        if (expr_type) error(4);
        *ptr = '\0';
    } else if (c == TOKEN(STR)) {       /* STR$(n)                          */
        num_string(eval_sub(), ptr);
        if (expr_type) error(4);
    } else if (c == TOKEN(HEX)) {       /* HEX$(n) -> uppercase hex string  */
        ubint uval = (ubint)eval_sub();
        if (expr_type) error(4);
        sprintf(ptr, "%X", (unsigned)uval);
    } else if (c == TOKEN(UNS)) {       /* UNS$(n) -> unsigned decimal str  */
        ubint uval = (ubint)eval_sub();
        if (expr_type) error(4);
        sprintf(ptr, "%u", (unsigned)uval);
    } else { error(0); }

    expr_type = 1;
}

/*
 * do_arith() - apply binary operator to two bint operands.
 * All arithmetic wraps at 16-bit naturally because bint is int16_t.
 * Bitwise ops use ubint to avoid UB on signed types.
 */
static bint do_arith(int opr, bint op1, bint op2)
{
    switch (opr) {
    case ADD-(ADD-1): return (bint)(op1 + op2);
    case SUB-(ADD-1): return (bint)(op1 - op2);
    case MUL-(ADD-1): return (bint)(op1 * op2);
    case DIV-(ADD-1): if (!op2) error(5); return (bint)(op1 / op2);
    case MOD-(ADD-1): if (!op2) error(5); return (bint)(op1 % op2);
    case AND-(ADD-1): return (bint)((ubint)op1 & (ubint)op2);
    case OR -(ADD-1): return (bint)((ubint)op1 | (ubint)op2);
    case XOR-(ADD-1): return (bint)((ubint)op1 ^ (ubint)op2);
    case EQ -(ADD-1): return (bint)(op1 == op2);
    case NE -(ADD-1): return (bint)(op1 != op2);
    case LE -(ADD-1): return (bint)(op1 <= op2);
    case SHL-(ADD-1): return (bint)((ubint)op1 << op2);  /* logical shift */
    case LT -(ADD-1): return (bint)(op1 <  op2);
    case GE -(ADD-1): return (bint)(op1 >= op2);
    case SHR-(ADD-1): return (bint)((ubint)op1 >> op2);  /* logical shift */
    case GT -(ADD-1): return (bint)(op1 >  op2);
    default: error(0); return 0; }
}

/*
 * num_string() - convert bint to decimal ASCII.
 * cstack[6] is large enough: max 5 digits + sign for int16_t (-32768).
 */
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

/* =======================================================================
 * Memory housekeeping
 * ======================================================================= */

static void clear_pgm(void)
{
    struct line_rec *p, *next;
    for (p = pgm_start; p; p = next) { next = p->Llink; free(p); }
    pgm_start = NULL;
    pgm_end   = NULL;
}

static void clear_vars(void)
{
    ubint i;
    for (i = 0; i < NUM_VAR; ++i) {
        num_vars[i] = 0;
        if (char_vars[i]) { free(char_vars[i]); char_vars[i] = NULL; }
        if (dim_vars[i])  { free(dim_vars[i]);  dim_vars[i]  = NULL; } }
}

/* =======================================================================
 * get_var() - parse a variable name; return its index (0-259).
 * Sets expr_type: 0 = numeric, 1 = string ($-suffixed).
 *
 * Index encoding: (letter - 'A') * 10 + digit
 *   A/A0 -> 0,  A1 -> 1, ...,  Z9 -> 259
 * ======================================================================= */
static ubint get_var(void)
{
    tok_t c;
    ubint index;

    c = get_next();
    if (!isalpha((unsigned char)c)) error(0);
    index = (ubint)((toupper((unsigned char)c) - 'A') * 10);

    if (isdigit((unsigned char)*cmdptr)) {
        index = (ubint)(index + (*cmdptr - '0'));
        c = (tok_t)*++cmdptr;
    } else { c = (tok_t)*cmdptr; }

    if (c == '$') { ++cmdptr; expr_type = 1; }
    else          {           expr_type = 0; }

    return index;
}

/* =======================================================================
 * main()
 * ======================================================================= */
int main(int argc, char *argv[])
{
    int   i;
    ubint j;
    tok_t tok;

    /*
     * Copy command-line args into A0$, A1$... via strdup so clear_vars()
     * can safely free them.  argv[0] is the interpreter; BASIC args start
     * at argv[1].
     */
    pgm_start = NULL;
    pgm_end   = NULL;
    for (j = 0, i = 1; i < argc; ++i, ++j) {
        if (char_vars[j]) strcpy(char_vars[j], argv[i]); }

    /*
     * If argv[1] names a file, load and run it silently before the banner.
     * Programs terminating with EXIT produce no extra output.
     */
    if (j) {
        FILE *fp;
        snprintf(filename, sizeof(filename), "%s.BAS", char_vars[0]);
        if ((fp = fopen(filename, "rb")) != NULL) {
            while (fgets(buffer, (int)(sizeof(buffer)-1), fp)) edit_program();
            fclose(fp);
            if (!setjmp(savjmp)) execute((tok_t)RUN1); } }

    printf("%s %d.%d  (based on %s)\n",
           FORK_NAME, FORK_VER_MAJOR, FORK_VER_MINOR, BASE_VER_STR);
    printf("Copyright 1982-2003 Dave Dunfield. "
           "Modernized for GCC / ia16 / MinGW, %s.\n", BUILD_YEAR);

    setjmp(savjmp);
    for (;;) {
        fputs("Ready\n", stdout);
noprompt:
        mode    = 0;
        ctl_ptr = 0;
        { char *r = fgets(buffer, (int)(sizeof(buffer)-1), stdin); (void)r; }
        if (edit_program()) goto noprompt;
        tok = skip_blank();
        if (IS_TOK(tok)) { ++cmdptr; execute(tok); }
        else if (tok)    { execute((tok_t)LET); } }
}
