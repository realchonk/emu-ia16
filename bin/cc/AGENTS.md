# cc — the self-hosting C compiler

This directory is the driver for a small, self-hosting C compiler targeting
16-bit x86 (IA-16, 80286 protected mode) running under `emu`. The compiler is
split into four guest programs that communicate through temp files:

```
source.c
   |  cpp    (K&R preprocessor)
   v  source.i
   |  c0     (parser + type checker)      -> .ast  .str  .sym
   v
   |  c1     (AST -> SSA)                 -> .ssa  (uses .tmp scratch)
   v
   |  c3     (SSA -> NASM)                -> .asm
   v
  nasm -> .o -> xcc/link -> flat 16-bit binary -> emu
```

`cc` itself is `cc.c`; it only knows how to fork/exec the four passes and
manage the intermediate files. It is **not** a full cc: it accepts only `-k`
(keep temp files). There is no `-c`, `-o`, `-I`, etc. Do not assume otherwise.

## Build and run

From the repo root (needs `bmk`/`mk`, `nasm`, the `ia16-elf` toolchain, and the
host C compiler):

```sh
mk bin/cc/run        # builds cpp, c0, c1, c3, cc; compiles bin/cc/test.c and runs it
mk bin/cc/cc         # just the driver
mk                   # whole tree
```

`mk bin/cc/run` runs `cc test.c -k` under emu, assembles `test.asm` with `xcc`,
and runs the result. Expected output:

```
AAAAAAAA
BBBBBBBB
```

### Running passes by hand

`cc` and the four passes (`cpp`, `c0`, `c1`, `c3`) are themselves **flat 16-bit
guest binaries**, so they must be run through the host `emu` binary (`emu/emu`,
an x86-64 ELF) — the guest binaries are not executable directly. You can drive
each pass by hand:

```sh
E=./emu/emu; L=./lib
$E $L/cpp/cpp -I$L/libc/include foo.c foo.i
$E $L/c0/c0  foo.ast foo.str foo.sym < foo.i
$E $L/c1/c1  foo.tmp < foo.ast > foo.ssa
$E $L/c3/c3  foo.str foo.sym < foo.ssa > foo.asm
nasm -felf32 foo.asm -o foo.o
```

`cc`'s embedded pass paths are `./../../lib/...`, relative to the CWD, so
`emu ./cc` only resolves its passes when run from `bin/cc` (which is why the
`run` rule does `${EMU} ./cc ...` there). A symlink `bin/cc/lib -> ../../lib`
makes running `emu bin/cc/cc` in place work, but do not commit one.

## The compiler is K&R C only

c0 implements K&R C with structs/unions: no prototypes as a type (only
identifier lists), implicit int, no `const` enforcement, function definitions
use old-style parameter declarations (`f (a, b) int a; char *b; { ... }`).
Any compound block may open with declarations, then statements (K&R). At the
outer level the type may be omitted entirely (implicit int); inside a block a
specifier is required, as implicit int would be ambiguous with an expression
statement. `lib/cpp/yylex.c` and `lib/cpp/test.c` are not valid input for this
reason (no-prototype / preprocessor-only).

## Intermediate formats

The authoritative spec lives in `bin/cc/ast-fmt` (kept in-tree, updated with the
IR). Summary:

- **`.ast`** — c0's tree IR. Magic `CIR\0`. Expressions are tagged one byte
  (`i I S g l u b ? c = n , *`), followed by a `ty` byte and operands. The model
  is address-based: `g`/`l` yield the **address** of a global/local, `*` loads
  through an address, `=` stores. Pointer arithmetic is pre-scaled to bytes by
  c0. Reading a variable is `* (g x)`; arrays/functions decay to a bare
  address. `ty`: `[1:0]` width (0=byte,1=word,2=dword,3=block), `[2]` unsigned;
  width 3 is followed by a word byte count.
- **`.str`** — NUL-separated strings. A `six` is a byte offset into it.
- **`.sym`** — NUL-separated symbol names. A `sym` is a byte offset; if the high
  bit is set it is a generated label `.LN` (low 15 bits) with no file entry.
- **`.ssa`** — c1's output, c3's input. Magic `CIR\001`.

### SSA design (c1 -> c3)

The SSA form uses **block arguments instead of phi nodes**: a label declares
`nparams` values and each `J`/`B` binds them positionally, like a call. This
makes the parallel copy a phi implies explicit on the edge. Values are 0-based
ids; c3 assigns physical homes.

- Statements: `L sym n p...` (label + params), `J sym n a...`, `B cond tsym tn
  t... fsym fn f...`, `R src`, `r`.
- Values: `i size dst imm`, `S dst six`, `g dst sym` (address), `l dst slot`
  (address), `C flags dst src` (cast: 0 trunc, 1 zext, 2 sext), `* size dst src`
  (load), `= size dst src` (store), `u size op dst src`, `b flags op dst a b`
  (`[1:0]` size, `[2]` signed, `[3]` b is immediate), `c size n dst func
  (size arg)...`.
- Function record: `F flags sym args 0xffff vars 0xffff code 0xff regs 0xffff
  0xff`. `l slot` indexes `args ++ vars`.
- c0's own control flow produces labels with 0 params; only the synthetic merge
  blocks for `&&`, `||` and `?:` (built in `lib/c1/ssa.c`) have 1 param.

### c3 (SSA -> asm)

Linear-scan allocation over conservative live intervals (`[first def, last
use]`), spilling to bp-relative slots. cdecl ABI: args at `[bp+4]...`, caller
cleans up, result in AX, SI/DI callee-saved (always saved at `[bp-2]`/`[bp-4]`).
Address-class values (used as load/store addresses) are restricted to
`addrreg`-capable registers (AX/BX/SI/DI). Block-argument moves are emitted on
edges by `edge()`, assuming at most one parameter on acyclic edges.

## Bugs found and fixed (keep these in mind)

- **c3 store intervals**: `'='` must be handled like a *use* of both the address
  and the value, with the **address** tagged `CL_ADDR` — not like a load. The
  old shared `'*'/'='` case marked the address as defined and the value as
  `CL_ADDR`, corrupting allocation (e.g. `i = 1` stored the address into
  itself). See `compute_intervals()` in `lib/c3/emit.c`.
- **c0 streams records inline**: c0 writes static-local data records (`D`) *and*
  in-body function declarations (`F ... \xff`) **inside** a function's code
  stream, before the enclosing `F`'s trailing `0xff`. c1's `stmt()` must handle
  `case 'D'` (emit via `def()`) and `case 'F'` (emit via `fdecl()`), returning
  the next statement. Missing this yields `S: invalid type=44` (`','`) or `=46`
  (`'.'`).
- **c1 streams SSA code to disk**: c1 used to buffer a whole function's code in
  `cbuf[CBUF]` and error `ssa: code too large`. It now writes code bytes to the
  `tfd` scratch file (`ecb()` writes to `tfd`), rewinds, lowers, then copies the
  code back between the `F` header and the value table. The value table
  (`vsz[NVALUE]`) is still in core. `CBUF` is gone.

## Capacity limits (guest is a 64 KiB segment)

All passes are flat 16-bit programs sharing one 64 KiB segment, so pools are
sized statically and raising one costs bss. Current pools and the peaks seen
compiling this tree:

| pass | pool | value | peak |
|------|------|-------|------|
| c1 | `NSPOOL` stmts | 640 | 608 |
| c1 | `NEPOOL` exprs | 2304 | 2169 |
| c1 | `NVALUE` SSA values | 2304 | 2242 |
| c3 | `MAXINS` insns | 1700 | 2568 |
| c3 | `MAXVAL` values | 1200 | 2242 |
| c3 | `ARGPOOL` edge/call args | 3072 | 342 |
| c3 | `MAXLOC` slots | 48 | 15 |

Peaks measured compiling this tree's largest files (`lib/c3/emit.c`,
`lib/cpp/cpp.c`, `bin/yacc/y2.c`). c3's `MAXINS`/`MAXVAL` are the current
blockers: `lib/c3/emit.c` and `lib/cpp/cpp.c` fail with `too many instructions`,
`bin/yacc/y2.c` with `too many values`. Raising them costs bss (one
`struct insn` per `MAXINS` entry, one byte per `MAXVAL`), so it needs the same
budget treatment as c1.

## Testing the compiler

There is no unit-test harness. The practical check is to compile the tree's own
`.c` files through the pipeline. The tree has ~81 `.c` files; as of the SSA
rewrite **76 compile through c1** and **73 all the way to an object**. The
known non-compiling set is environmental, not compiler bugs:

- `bin/yacc/y1.c` — needs `-DPARSER="..."` (set by `bin/yacc/Mkfile`).
- `emu/emu.c`, `emu/sys.c` — include host glibc headers (not guest code).
- `lib/cpp/test.c` — a preprocessor-only test, not C.
- `lib/cpp/yylex.c` — K&R no-prototype file (missing types).

Compile the whole tree with a small driver that runs each pass under `emu`
(from a CWD at the right depth, or with the passes' paths fixed up). Always
check a built program actually runs, not just that it assembles — the `i = 1`
allocation bug produced valid-looking asm that stored the wrong value.

## Code conventions

- K&R-style C in the passes (old-style function definitions, `register`,
  implicit int) — match the surrounding files, not modern C.
- Passes are freestanding and avoid stdio where practical: c0/c1 use raw fd
  I/O (`read`/`write`/`oputc`). c3 is the exception and uses `printf`/`err`
  (it is the least size-constrained).
- Keep pools static; do not `malloc` in the guest passes.
- Comments explain the IR/format and invariants, not the obvious.
- `.gitignore` already ignores `*.i *.ast *.str *.sym *.ssa *.tmp *.asm` and the
  built binaries; scratch files won't be committed.
