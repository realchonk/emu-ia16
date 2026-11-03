# `bin/ld` — project linker design

Status: **design only**.  This document describes a linker that can replace
the host `ia16-elf-ld` step in `xcc` and later run as a guest program under
`emu`.  No compiler or assembler code is changed by this design.

The target is deliberately narrow: a single 16-bit IA-16 segment, ELF32
relocatable inputs, two relocation types, a flat output image, no dynamic
linking, and a fixed, tiny linker-script subset.  That is exactly what the
current toolchain needs.

---

## 0. Summary of decisions

| Area | Decision |
| --- | --- |
| Input | ELF32 `ET_REL`, little-endian, `EM_386`; `SHT_PROGBITS`, `SHT_NOBITS`, `SHT_REL`; symbols and archives |
| Relocations | `R_386_16` (20) and `R_386_PC16` (21) only |
| Output | Headerless flat binary, byte 0 = guest address 0, default format |
| Entry | `_start` must resolve to address 0 (emu always enters at IP 0) |
| Layout | Four output sections, in script order: `.text`, `.rodata`, `.data`, `.bss`; `__brk`, `__brk_end`; stack is reserved from the top |
| Symbol model | Strong/weak/undefined/common/absolute/script symbols; locals and section symbols are object-private |
| Key correctness point | A section symbol resolves to the **address of that input section**, not to the output section base |
| Archives | Required for the current `-lc -lgcc` link; support in the host linker, initially avoid in the guest linker |
| Implementation | Two passes, static tables, no `malloc`, 32-bit file offsets, same K&R C source for host and guest |
| Budget | guest linker target: <= 28 KiB text, <= 24 KiB static data, 2 KiB stack |

The most important non-obvious rule is the section-symbol rule above.  Both
`c4` and `bin/as` use *section symbols plus an addend* for every reference to
a symbol defined in the same object, instead of naming that symbol in the
relocation.  A linker that resolves a section symbol to the output section
base will silently produce wrong code whenever an object's `.text` (or
`.bss`, `.rodata`, `.data`) is not placed at offset 0 of its output section.

---

## 1. Input object model

### 1.1 ELF envelope

Both current object producers write the same basic envelope:

* `e_ident`: `7f 45 4c 46`, class 1 (ELF32), data 1 (little-endian), version 1.
* `e_type`: `ET_REL` (1).
* `e_machine`: `EM_386` (3).
* `e_version`: 1.
* `e_entry`: 0 (irrelevant for `ET_REL`).
* `e_phoff = 0`, `e_phentsize = 0`, `e_phnum = 0`.
* `e_ehsize = 52`.
* `e_shentsize = 40`.
* `e_shnum = 7` for c4/as objects.
* `e_shstrndx = 6`.
* `e_flags = 0`.

`lib/c4/main.c` builds this in `ehdr[]` (lines 1113-1118) and
`bin/as/as.c` in `ehdr[]` (lines 1089-1094).  The first section data
(`.text`) starts at file offset 64 (`textoff = 64`;
`lib/c4/main.c:1758`, `bin/as/as.c:1333`); the section header table is
written at `shdroff` near the end of the file.  The 52-byte ELF header leaves
12 unused bytes before `textoff`.

The linker must parse by **section type and name**, never by fixed section
number, because host-produced objects (and `crt0.o` from nasm) use different
orders.

### 1.2 Sections emitted by `c4` and `bin/as`

Both producers write exactly seven sections:

| Nr | Name | Type | Flags | Offset | Size | Link | Info | Align | Entsize |
| ---: | --- | --- | --- | --- | --- | ---: | ---: | ---: | ---: |
| 0 | — | `SHT_NULL` | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| 1 | `.text` | `SHT_PROGBITS` | 6 (`AX`) | 64 | text size | 0 | 0 | 16 | 0 |
| 2 | `.bss` | `SHT_NOBITS` | 3 (`WA`) | 64 + text size | bss size | 0 | 0 | 4 | 0 |
| 3 | `.rel.text` | `SHT_REL` | 0 | `(64+text+3)&~3` | `nrels*8` | 4 | 1 | 4 | 8 |
| 4 | `.symtab` | `SHT_SYMTAB` | 0 | after relocs | `nsyms*16` | 5 | 3 | 4 | 16 |
| 5 | `.strtab` | `SHT_STRTAB` | 0 | `stroff` | `strsize` | 0 | 0 | 1 | 0 |
| 6 | `.shstrtab` | `SHT_STRTAB` | 0 | `shstroff` | 48 | 0 | 0 | 1 | 0 |

(References: `write_shdrs()` in `lib/c4/main.c:1127-1159` and
`bin/as/as.c:1124-1135`; section header string offsets are the fixed
`shstr[48]` arrays at `lib/c4/main.c:1107-1111` and
`bin/as/as.c:1083-1087`.  The symbolic table above gives the actual names and
sizes; the `Link`/`Info` columns are from the `put_shdr()` calls.)

Important details:

* `.text` is the only file-backed, executable section.  All initialised data
  and all strings are placed in `.text` by both producers.
  - `c4`: `mddata()` selects `.text` when `(flags & 4)` is set
    (`lib/c4/main.c:1601`); `mdstr()` always sets `cursec = 0`
    (`lib/c4/main.c:1687-1690`).
  - `as`: `switchsec()` sets the section to `.bss` only when
    `name[1] == 'b'`, otherwise `.text` (`bin/as/as.c:337-342`).  Thus
    `section .rodata` and `section .data` both become `.text`.
* `.bss` is `SHT_NOBITS`; its `sh_offset` is not meaningful as file data and
  is often not aligned (`64 + text_size` is not rounded).  Never read bytes
  from it; use `sh_size` only.
* `.text` has `sh_addralign = 16`.  This is larger than host objects' usual
  `1` and will introduce padding when several c4/as `.text` sections are
  concatenated.  The linker must honour each input section's alignment.
* Both producers always emit all seven sections, even when `.bss`, `.rel.text`
  or the symbol table are empty.
* Object files are limited to 64 KiB by the 16-bit `shoff`, `sh_size`,
  `r_offset` and symbol values used by both writers.  The linker should reject
  an input section or symbol value that does not fit in 16 bits unless it is
  explicitly an absolute/archive offset.

### 1.3 Symbols emitted by `c4` and `bin/as`

Symbol table entry 0 is the null entry.  Entries 1 and 2 are section symbols:

* index 1: `.text` (`STB_LOCAL`, `STT_SECTION`, `st_value = 0`, `st_shndx = 1`)
* index 2: `.bss` (`STB_LOCAL`, `STT_SECTION`, `st_value = 0`, `st_shndx = 2`)

This fixed section-symbol numbering is used by the relocation writer:
`addrel()` writes `esym = 1 + (target & 1)` for a section target
(`lib/c4/main.c:398-401`, `bin/as/as.c:405-408`).  It is correct for these
objects only because `.text` is section 1 and `.bss` is section 2.

All remaining emitted symbols are written with:

* `st_info = 0x10` — `STB_GLOBAL`, `STT_NOTYPE`
* `st_other = 0`
* `st_size = 0`
* `st_value` = section-relative offset, or 0 if undefined
* `st_shndx` = section index + 1 for defined symbols, or 0 for undefined.

There are **no local defined symbols** in the emitted table and no `STT_FILE`
entry.  The selection rule is `emit_sym()`:

* defined symbol: emitted only if `F_GLOBAL` is set
  (`lib/c4/main.c:1162-1168`, `bin/as/as.c:1138-1145`);
* undefined symbol: emitted if any of `F_GLOBAL`, `F_EXTERN` or `F_REF` is set.

Consequences:

* A `static` function or `static` data object is defined in the internal pool
  but is **not written to `.symtab`**.  References to it from the same object
  become section-symbol relocations with the symbol's offset as addend.
  This is why `nm` does not show c4/as static symbols.
* `F_REF` is set when a name is first created, so a referenced undefined
  symbol is emitted.  `extern` declarations can also create an undefined
  symbol through `F_EXTERN` (`marksym()`; `lib/c4/main.c:318-328`,
  `bin/as/as.c:325-335`), even if no relocation references it.  Older
  objects in the tree show many such unreferenced undefined symbols.  The
  linker should drive archive extraction and undefined checks from
  **relocations**, not from the mere presence of `SHN_UNDEF` symbol entries;
  otherwise a stale object can pull in the whole C library (see §6).
* The statement “all symbols are ELF global notype, even static functions”
  is slightly imprecise: static symbols are *omitted*, while all symbols that
  are emitted are global notype.  The practical effect for a linker is the
  same: do not rely on `st_info` type, `st_size`, or on local defined symbols
  being present.

`c4` and `as` also handle local labels internally:

* `c4` generated labels use the high bit `0x4000` in its internal symbol
  representation (`findlbl()`, `lib/c4/main.c:1357-1377`).  They are never
  written to `.symtab`.
* `as` treats any name beginning with `.` or containing `.L` as local
  (`findsym()`, `bin/as/as.c:273-290`).  These are also never written.
* Forward local references are recorded in `fixes[]` and patched by
  `resetlocals()` (`lib/c4/main.c:250-271`, `bin/as/as.c:250-271`).
  PC-relative fixups are computed as section-relative displacements; absolute
  fixups are patched with the section-relative label offset.  There is no
  relocation entry left behind for them.

### 1.4 Relocations emitted today

Only two types occur, in `.rel.text` (c4/as) or any `.rel.*` section (host
objects):

* type 20 = `R_386_16`
* type 21 = `R_386_PC16`

`addrel()` writes an 8-byte `Elf32_Rel`:

```
r_offset = section-relative offset of the 2-byte field
r_info   = (symbol_index << 8) | type
```

There is no `r_addend`; this is REL, not RELA.  The addend is the existing
16-bit value in the field.

How c4/as construct the field and relocation:

| Source construct | Field value written | Relocation target |
| --- | --- | --- |
| undefined global, ABS16 | `val` | named global symbol |
| undefined global, PC16 | `val - 2` | named global symbol |
| defined target, ABS16 | `symval(sym) + val` | section symbol for target's section |
| defined target, other section, PC16 | `symval(sym) + val - 2` | section symbol for target's section |
| defined target, same section, PC16 | `symval(sym) + val - (off+2)` | **none** (encoded directly) |
| forward local label, ABS16 | `0`, then patched by `resetlocals()` | none (internal fixup) |
| forward local label, PC16 | `0`, then patched by `resetlocals()` | none (internal fixup) |

References: `emit_abs()` and `emit_pc()` in `lib/c4/main.c:418-492` and
`bin/as/as.c:425-499`; `addrel()` in `lib/c4/main.c:391-416` and
`bin/as/as.c:398-423`.

The `-2` in PC-relative fields accounts for the fact that `P` is the address
of the relocation field, while the CPU computes the branch target from the
end of the 16-bit displacement.  All current producers, including host
`ia16-elf-gcc`, encode `0xfffe` (−2) for ordinary calls and jumps.  The linker
must use the ELF formula `S + A - P`, not assume the addend is zero.

### 1.5 Edge cases and current inconsistencies

These are not hypothetical; they affect the parser and the layout engine.

1. **Section order and section-symbol numbers differ.**
   c4/as: `.text` is section 1, `.bss` is 2, and the two section symbols
   happen to be symbol entries 1 and 2.  nasm's `crt0.o` has `.bss` as
   section 1 and `.text` as section 2, and its section symbols are entries
   2 (`.bss`) and 3 (`.text`) because entry 1 is `STT_FILE`.  Resolve
   sections through `st_shndx` and section names, never through a fixed
   symbol index 1/2.

2. **Only two sections in c4/as, four or more in host objects.**
   c4/as never emit `.rodata` or `.data`.  `as` folds every non-`.bss`
   section name into `.text`.  Host `ia16-elf-gcc` emits `.text`,
   `.text.startup`, `.rodata`, `.rodata.str1.1` (SHF_MERGE|STRINGS),
   `.data`, `.bss`, `.comment`, and matching `.rel.*` sections.

3. **Relocation sections are not always `.rel.text`.**
   Host objects also have `.rel.rodata`, `.rel.data`, `.rel.text.startup`.
   Use each `SHT_REL` section's `sh_info` field as the target section, and
   `sh_link` as the symbol table.

4. **A 1-byte symbol reference can produce a 2-byte relocation.**
   `emit_abs()` writes `addrel()` and then only one byte when `size == 1`
   (`lib/c4/main.c:452-455`, `bin/as/as.c:459-462`).  `as` can do this for
   `db symbol`.  The resulting `R_386_16` would overwrite the following byte.
   The linker must validate `r_offset + 2 <= target_size` and reject such an
   object, or the producers must stop emitting this combination.

5. **Absolute local fixups assume the object's section starts at 0.**
   `resetlocals()` patches absolute forward local labels with the
   section-relative value, with no relocation entry.  This is safe for
   PC-relative labels (and is what c3 generates), but an absolute
   `dw .Lfoo` in `as` input is baked at object-relative offset.  If that
   object's `.text` is placed at a nonzero address in the output, the value is
   wrong and the linker cannot detect or repair it.  Do not implement `-r`
   for these objects until the producers emit a real relocation for absolute
   local references.  c3's compiler-generated code does not currently use
   absolute local labels.

6. **Alignment is not uniform.**
   c4/as `.text` has `sh_addralign = 16`; host `.text` normally has 1.
   `.rodata` may have 4, `.data` 2, `.bss` 2 or 4.  Honouring alignment can
   introduce padding between input sections; ignoring it changes every
   subsequent symbol address.

7. **Symbol values, sizes and relocation offsets are 16-bit in the producers.**
   The ELF fields are 32-bit, but c4/as write only the low 16 bits.  Validate
   that they fit; do not silently truncate.  Archive member offsets, by
   contrast, can exceed 64 KiB (`libgcc.a` is 148382 bytes), so the linker's
   own file offsets must be 32-bit.

8. **Undefined symbols may be present without a relocation.**
   See §1.3.  They are harmless for a final link unless the linker uses the
   symbol table as its undefined set.  Use relocation-referenced symbols.

9. **There are no `STT_OBJECT`/`STT_FUNC` types and no sizes.**
   Debug information and garbage collection are not possible from these
   objects without producer changes.  The linker must not use `st_size` to
   place a symbol; only section + `st_value`.

10. **`.bss` has no file bytes and may have an unaligned `sh_offset`.**
    `SHT_NOBITS` is memory only.  It still contributes to `__brk` and to the
    address assigned to input section symbols.

11. **`c4` puts initialised data and strings in `.text`.**
    There is no read-only/data separation in self-hosted objects; this is
    fine because `emu` maps the whole 64 KiB segment RWX.  A future producer
    may add `.rodata`/`.data`; the linker design already supports them.

12. **`as` recognises a section as `.bss` by the second character.**
    `section .bss2` also maps to `.bss`; `section .data` maps to `.text`.
    This is permissive but can surprise a script that expects `.data`.

### 1.6 Host objects that the current `xcc` link actually consumes

The current `xcc` uses `ia16-elf-gcc` for `.c` and `nasm -felf32` for `.asm`,
so stage 1 must also handle the host object model:

* `lib/crt0.o` (nasm): `.bss` then `.text`; `STT_FILE`; 5 relocations
  (3 ABS16 to `.bss`, 2 PC16 to `main`/`_exit`); `.text` align 16, `.bss`
  align 4; no `.rodata`/`.data`.
* `ia16-elf-gcc` objects: `STT_FILE`, `STT_SECTION`, `STT_FUNC`,
  `STT_OBJECT`, local and global symbols, real sizes; `.rodata.str1.1`
  with `SHF_MERGE|SHF_STRINGS`; `.comment` (non-alloc, discard); COMMON
  symbols; only `R_386_16`/`R_386_PC16` relocations.
* `lib/libc/libc.a`: 78 members, 59934 bytes total.  Aggregate payloads:
  `.text` 6177 bytes, `.rodata` 403, `.rodata.str1.1` 772, `.data` 8,
  `.bss` 12, `.comment` 1098.  It has four `SHN_COMMON` symbols
  (`errno`, `optind`, `optarg`, `optopt`), all size 2/alignment 2.
  Relocations: 177 `R_386_16`, 128 `R_386_PC16`.
* `libgcc.a` (from `ia16-elf-gcc -print-libgcc-file-name`):
  148382 bytes, 156 members.  Aggregate `.text` is 42220 bytes, but only a
  few members are needed by current libc (`udivsi3.o`, `umodsi3.o`,
  `ia16-ldivmodu.o`; `__udivsi3`/`__umodsi3` call `__ia16_ldivmodu`).
  Relocations: 88 `R_386_16`, 715 `R_386_PC16`.

The aggregate libc text is only ~6 KiB, so an archive-free stage-2 link is
feasible if desired (§6.5), but the current `xcc` command explicitly uses
`-lc -lgcc` and should be supported by the host linker.

---

## 2. Output expected by `emu`

### 2.1 What `emu` actually does

`emu/emu.c`:

* `setup_text()` (`emu/emu.c:148-167`) mmaps a **zero-filled 64 KiB**
  region (`PROT_READ|PROT_WRITE|PROT_EXEC`, `MAP_32BIT`) and installs a
  16-bit code segment and a 16-bit data segment covering all 65536 bytes.
* `main()` (`emu/emu.c:277-313`) opens the program, reads it from offset 0
  into `text`, and stops at EOF or 64 KiB.
* `enter16()` (`emu/emu.c:215-235`) jumps to `IP = 0` (`entry = {0,0}`) in
  that code segment.  It does **not** read an ELF header, program header,
  entry point, or load address.
* `setup_args()` (`emu/emu.c:169-213`) builds a normal process stack at the
  **top of the segment**: argument strings, a NULL-terminated `argv[]`, and
  `argc`; it returns `SP`.  The hardware stack grows down from there.
* The guest's `crt0` receives the syscall shim selector in `AX:CX`
  (`lib/crt0.asm:12-33`), stores it in `_sys`, and calls `main(argc, argv)`.

Therefore the output format is simply:

```
file offset 0  == guest address 0
file offset N  == guest address N
no header, no segment table, no padding beyond what layout requires
```

File length is the end of the highest file-backed allocated section.  The
rest of the guest segment is zero because the mmap is anonymous and
zero-filled.  A file larger than 65536 is silently truncated by the current
`emu` read loop, so the linker must reject such an output.

### 2.2 Deriving the raw image without `objcopy`

For the current script, the derivation is:

1. Put `.text` at 0, honouring input-section alignment.
2. Append `.rodata`, `.data` in script order, honouring alignment.
3. Do **not** emit `.bss` bytes (it is `SHT_NOBITS`); reserve its address
   range in memory only.
4. Write file bytes in increasing address order.  If a file-backed section
   starts after a gap (including a skipped `.bss`), fill the gap with zero
   bytes.  Stop at the end of the last file-backed section.
5. `__brk` and `__brk_end` are symbol values only; they are not written into
   the file.  All relocations against them have already been applied at link
   time.

This is exactly what `ia16-elf-objcopy -O binary` does for a script-generated
ELF with VMA == LMA and one load segment.  The host ELF's file offsets,
program headers, section headers and symbol table do not participate in the
raw image.

### 2.3 Concrete reference layout

A current host link of `lib/crt0.o + bin/hello/hello.o + libc.a + libgcc.a`
with `STACK_SIZE=2048` produces:

| Item | Address | Size | Notes |
| --- | ---: | ---: | --- |
| `.text` | 0x0000 | 0x1c8e | align 16, starts at 0 |
| padding | 0x1c8e | 0x2 | to align `.rodata` to 4 |
| `.rodata` | 0x1c90 | 0x46c | includes `.rodata`/`.rodata.str1.1` (GNU ld may merge strings) |
| `.data` | 0x20fc | 0x8 | |
| `.bss` | 0x2104 | 0x1c | not in file |
| `__brk` | 0x2120 | — | after `.bss` |
| `__brk_end` | 0xf800 | — | `0x10000 − 2048` |
| raw file size | — | 0x2104 | 8452 bytes |

The raw file ends at 0x2104, while `__brk` is 0x2120; the 28-byte `.bss`
range is supplied by the zero-filled mapping.  `emu` loads this file
correctly; no ELF or `objcopy` step is needed.

A mixed link of `crt0.o + bin/cc/test.o` (c4 object) with the same libraries
gives a smaller example:

| Item | Address | Size |
| --- | ---: | ---: |
| `.text` | 0x0000 | 0x730 |
| `.data` | 0x730 | 0x6 |
| `.bss` | 0x738 | 0xc (2 bytes of alignment padding before it) |
| `__brk` | 0x744 | — |
| raw file size | — | 0x736 (1846 bytes) |

This confirms that the same linker must handle both c4/as objects and host
objects, and that section-symbol addresses must include each input section's
placement offset.

### 2.4 Entry, load address, stack, brk

* **Load address:** 0.  The first byte of the file is guest address 0.
* **Entry:** `IP = 0`.  `_start` must be at address 0.  `xcc` passes
  `$crt0` first, so its `.text` starts at 0 and `_start` is its first symbol.
  The linker must check `ENTRY` / `--entry` and report an error if the chosen
  entry symbol does not resolve to 0.  A script `ENTRY(_start)` does not move
  anything by itself; it only names the entry.
* **Stack:** not encoded in the file.  `emu` builds args at the top and sets
  SP there.  `-mstack=N` only controls the reserved top region via
  `__brk_end = 0x10000 - N`, and the linker-script assertion.
* **Brk:** `lib/libc/unistd/brk.c:4-25` defines
  `static void *__brkp = __brk;` and rejects `addr > __brk_end`.
  `lib/libc/stdlib/malloc.c` calls `sbrk()` starting from `__brk`.
  So `__brk` is the initial heap address and `__brk_end` is the heap limit.
  Both must be valid 16-bit addresses because relocations are `R_386_16`.
  In particular, `STACK_SIZE=0` yields `__brk_end = 0x10000`, which cannot be
  encoded; treat it as an error (GNU ld reports a truncated relocation).
* **Assertion:** `ASSERT(__brk <= __brk_end, ...)` must be evaluated after
  layout.  Also reject `__brk > 0xffff`, `__brk_end > 0xffff`, and
  `file_end > __brk` (which would mean file-backed data overlaps the heap).
  The current script has `.bss` last, so `file_end <= __brk`.

---

## 3. Linker-script / memory-layout semantics for `lib/user.x`

`lib/user.x` is:

```ld
ENTRY(_start)

SECTIONS {
	.text : { *(.text*) }
	.rodata : { *(.rodata*) }
	.data : { *(.data*) }
	.bss : {
		*(COMMON)
		*(.bss*)
	}
	__brk = .;
	. = 0x10000 - STACK_SIZE;
	__brk_end = .;
	ASSERT (__brk <= __brk_end, "Not enough size for stack!")
}
```

### 3.1 Required semantics

* Initial location counter is 0.
* Output sections appear in script order.  Each output section starts at the
  current location counter.  Input sections are appended in **link order**
  (command-line object order, then archive-member selection order) and, within
  one object, in **section-header-index order**.
* An output section's alignment is the maximum `sh_addralign` of the input
  sections matched to it (and of any common symbols, for `.bss`).  Align the
  location counter to that value before placing the first input section; GNU
  ld aligns the output section start this way.  Then, before each individual
  input section, align the counter to that section's `sh_addralign` again.
  Alignment 0 or 1 means no padding.
  - Example from the host hello link: `.text` ends at 0x1c8e, the `.rodata`
    output section has max input alignment 4, so `.rodata` starts at 0x1c90,
    not 0x1c8e.
* A pattern `*(.text*)` matches `.text` and any name beginning `.text`
  (including `.text.startup`).  `*(.rodata*)` matches `.rodata` and
  `.rodata.str1.1`.  `*(.data*)`, `*(.bss*)` similarly.  Only a trailing `*`
  wildcard is needed for the current tree.
* `*(COMMON)` is a pseudo-input: allocate all common symbols in the `.bss`
  output section **before** the `.bss*` input sections.  Align dot to each
  common symbol’s required alignment before assigning its address; this
  matches the current final layout (`errno`, etc. before
  `_sys`/`__progname`).
* `.text` starts at 0.  A `.text` input section with `sh_addralign = 16`
  therefore starts at 0 if it is first; subsequent ones are padded to 16.
* `.bss` is `SHT_NOBITS`: it advances the location counter and assigns
  addresses but contributes no file bytes.
* `__brk = .` defines an absolute symbol equal to the current dot after the
  last `.bss` input (or after common if `.bss` is empty).
* `. = 0x10000 - STACK_SIZE;` sets the location counter without creating an
  output section.  `STACK_SIZE` comes from `--defsym`/`-mstack`, default 2048.
* `__brk_end = .;` defines an absolute symbol at that value.
* `ASSERT(expr, "message")` is evaluated after all layout.  On failure, print
  the message and exit nonzero.  The exact current message is
  `Not enough size for stack!`.
* `ENTRY(_start)` records the entry symbol.  It is not a layout directive and
  the raw output has no entry field.  Validate the final value is 0.

### 3.2 Internal layout algorithm

Two deterministic passes are enough:

```
pass A (layout):
  dot = 0
  for each output section in script order:
      out.align = max(sh_addralign of matched inputs, common align for .bss)
      dot = align(dot, out.align)
      out.start = dot
      for each input object in link order:
          for each input section in object header order:
              if section is alloc and name matches this output section:
                  dot = align(dot, section.align)
                  section.addr = dot
                  dot += section.size
      out.end = dot
  allocate COMMON entries at the point where *(COMMON) appears,
      aligning dot to each common symbol's st_value before assigning
  evaluate `.` assignments, __brk, __brk_end, ASSERT
  assign final addresses to global definitions:
      value = section.addr + st_value  (or common/script/absolute value)

pass B (write + relocate):
  write file-backed sections in address order, zero-filling gaps
  for each object and each SHT_REL section:
      target_sec = sh_info
      for each relocation entry: resolve and patch at
          target_sec.addr + r_offset
```

No per-relocation layout search is needed if the linker keeps a 16-bit
`section.addr` for every alloc input section.  With at most a few hundred
input sections this is cheap (e.g. 512 × 2 bytes = 1 KiB).

### 3.3 Script subset and restrictions

A full GNU linker-script parser is not needed and would not fit the guest
budget.  Implement this subset, and fail loudly on anything else:

```
script      := ENTRY '(' name ')' SECTIONS '{' item* '}'
item        := outsec | assignment | assert
outsec      := name ':' '{' input* '}'
input       := '*' '(' pattern ')' | '*' '(' COMMON ')'
assignment  := name '=' expr ';' | '.' '=' expr ';'
assert      := ASSERT '(' cmp [ ',' string ] ')'
expr        := term (('+' | '-') term)*
cmp         := expr [ ('<' | '<=' | '=' | '!=' | '>=' | '>') expr ]
term        := number | name | '.'
number      := decimal digits | '0x' hex digits
pattern     := exact-name or prefix followed by '*'
```

Supported output attributes: none.  Reject `AT`, `>`, `ALIGN`, `SUBALIGN`,
`PROVIDE`, `KEEP`, `SORT`, `INCLUDE`, `MEMORY`, `PHDRS`, etc.  The only
script in the tree needs none of them.  If `-T` is omitted, use a built-in
equivalent of `user.x`.

Unknown non-alloc sections (`.comment`, `.symtab`, ...) are ignored.
Allocated input sections that do not match any output section are an error
(“no output section for …”); silently dropping them would produce broken
code.  `.rel.*` sections are never output; they are processed as relocations.

### 3.4 Alignment and ordering compatibility note

Byte-for-byte compatibility with GNU ld is possible for a fixed input order
if the linker uses the same “object order, then section index order” rule,
aligns each output section to the maximum input alignment, and honours each
individual `sh_addralign`.  It will not be byte-identical in all cases
because GNU ld may merge `.rodata.str1.1` strings, may reorder orphan
sections, and may choose a different archive member order.  The acceptance
criterion should be: same section/symbol addresses where the script is
deterministic, same behaviour under `emu`, and intentional differences
documented (string merging, archive extraction).  For the c4/as-only world
there is no `.rodata.str1.1`, so exact parity is realistic.

---

## 4. Symbol resolution

### 4.1 Symbol classes

The linker needs these internal kinds:

| Kind | Source | Address used by relocation |
| --- | --- | --- |
| `UNDEF_REF` | referenced undefined global | must be resolved; error if not |
| `DEF_STRONG` | global/weak definition in an input object | final address |
| `COMMON` | `SHN_COMMON`, `st_value = align`, `st_size = size` | allocated `.bss` address |
| `ABS` | `SHN_ABS` input symbol or script/`--defsym` | literal value |
| local defined | `STB_LOCAL` non-section symbol | input-section address + `st_value` |
| section symbol | `STT_SECTION` | **input-section address** + `st_value` |
| file symbol | `STT_FILE` | never a relocation target; ignore |

Only `STB_GLOBAL` and `STB_WEAK` symbols enter the cross-object namespace.
`STB_LOCAL` symbols are private to their object.  A local with the same name
in two objects is not a conflict.

### 4.2 Global/static/extern

* **Global definition:** add to the global table.  If an existing strong
  definition exists and it comes from a different object, report a duplicate
  definition unless `--allow-multiple-definition` is given.  A strong
  definition overrides a previous common or weak definition.
* **Static definition:** c4/as omit it from `.symtab`; host objects may have
  a local defined symbol.  In both cases it is not exported.  Resolve
  references to it only within the object, using the object's section table
  and `st_value`.
* **Extern/undefined:** record it when a relocation references a global
  symbol with `st_shndx == SHN_UNDEF`.  Do not require it merely because a
  symbol table entry exists (§1.5 item 8).  Resolve from regular objects,
  then archives in command order, then script/`--defsym`.  Remaining
  referenced undefineds are a link error.
* **Unreferenced extern:** ignore for final undefined checks and archive
  extraction.  It may be reported with `-Wunused-extern` if desired.
* **Common:** coalesce by name.  If a strong definition appears, discard the
  common allocation.  Otherwise keep the maximum `st_size` and maximum
  `st_value` (alignment).  Allocate all commons before `.bss*` at the point
  of `*(COMMON)`.  This matters for `errno`/`getopt` in the current libc.
* **Weak:** weak definitions never conflict with strong; a weak undefined
  symbol resolves to 0 if nothing defines it, and it should not by itself
  force an archive member to be pulled.  No weak symbols occur in the current
  inputs, but the parser should accept the binding bits.

### 4.3 Section symbols and generated labels

This is the most important resolution rule:

> A relocation against a section symbol uses the final address of the
> **specific input section** identified by that symbol's `st_shndx`, plus the
> section symbol's `st_value` (normally 0), plus the addend in the field.
> It is *not* the output section base.

Example: `test.o` has `.text` at output offset 0x20.  Its relocation
`r_offset=0x301`, target `.text`, field `0x0201`; the correct target is
`0x20 + 0x201 = 0x221`.  Using output `.text` base 0 gives 0x201, which is
wrong.

For a named local defined symbol the same rule applies:
`section.addr + st_value`.  For a named global defined symbol, use the value
assigned to the global table (which itself was computed as
`section.addr + st_value`).  For a section symbol with a nonzero `st_value`,
add it too.

Generated/local labels from c4/as have already been resolved inside the
producer and are not in `.symtab`.  There is nothing for the linker to do
except not to assume every label has a symbol table entry.  Host objects may
contain local labels in `.symtab`; resolve them through their section.

### 4.4 Duplicate/undefined diagnostics

* Duplicate strong definitions: report both object names (and archive member)
  and the symbol name.  Exit nonzero.
* Undefined references: list each name and the referencing object(s).  If
  several, report all (bounded buffer) and exit nonzero.
* An undefined weak symbol resolves to 0.
* A relocation to `STN_UNDEF` (symbol index 0) is invalid.
* A relocation to a `STT_FILE` symbol is invalid.
* A relocation whose target section is not allocated or was not matched is
  invalid.

### 4.5 `-r` / partial linking

Do not implement `-r` in the first version.

Partial linking would require:

* emitting a new `ET_REL` object with combined `.text`/`.bss` (and possibly
  `.rodata`/`.data`), a new symtab/strtab, and translated relocations;
* deciding whether to keep local symbols and how to rename them;
* preserving or re-encoding relocations against section symbols.

More importantly, c4/as have already baked forward local absolute fixups into
their sections (§1.5 item 5).  Combining their `.text` at a nonzero offset
would leave those values object-relative and incorrect, and the linker cannot
recover the missing relocation.  Partial linking is safe for host GCC/nasm
objects, but not for the current c4/as output unless the producers change.
It is not needed for archive handling: the linker can select archive members
and perform one final link.

---

## 5. Relocation processing

### 5.1 Formula

For each `SHT_REL` entry:

```
target_input_section = sh_info of the relocation section
P = target_input_section.addr + r_offset
A16 = little-endian uint16 at output address P       (for R_386_16)
Apc = little-endian int16  at output address P       (for R_386_PC16)
S = resolved symbol value
```

Symbol value `S`:

* section symbol: `input_section.addr + st_value`
* local defined symbol: `input_section.addr + st_value`
* global defined symbol: `global.value` (`section.addr + st_value`)
* common: allocated common address
* absolute/script/`--defsym`: literal value
* undefined: error

Then:

```
R_386_16:   V = S + A16
            require 0 <= V <= 0xffff
            write uint16(V)

R_386_PC16: V = S + Apc - P
            write uint16(V & 0xffff)
```

The PC-relative result is a 16-bit displacement.  In 16-bit protected mode
`EIP` wraps within the 64 KiB code segment, so any two addresses in the
segment are reachable; a displacement of `0x8000` means both +32768 and
−32768 depending on interpretation, and the CPU's 16-bit addition gives the
same target either way.  Do **not** reject a PC16 value merely because it is
outside −32768..32767.  Optionally provide `--strict-pc16` to match a
diagnostic-oriented GNU setting, but it is not required for correctness in
this one-segment model.

The ABS16 range check is essential: all addresses must fit in the guest's
16-bit offset.  `STACK_SIZE=0` produces `__brk_end = 0x10000`, which must be
rejected rather than truncated.

### 5.2 Addend signedness

The field bytes are just bits; interpret them differently per type:

* `R_386_16`: `A16` is an **unsigned** 16-bit offset/addend.  A section
  offset of 0x8000 means 32768, not −32768.
* `R_386_PC16`: `Apc` is a **signed** 16-bit addend.  The producers store
  `−2` as `0xfffe`; treating it as 65534 would be wrong.

### 5.3 Validation and edge cases

For every relocation:

* `r_offset + 2 <= target_section.size`;
* target section is `SHF_ALLOC` and matched to an output section;
* target section is not `SHT_NOBITS` (there are no bytes to patch);
* `r_info` symbol index < symtab entry count;
* type is exactly 20 or 21; reject `R_386_32` (1), `R_386_PC32` (2), TLS,
  GOT/PLT, etc. with “unsupported relocation type N” (do not silently
  truncate);
* the relocation section's `sh_link` points at a valid `SHT_SYMTAB`, and
  `sh_info` points at an allocated section;
* two relocations must not overlap the same two bytes; detect if practical
  (the current producers never do this, but `as`'s `db symbol` case could
  produce a 1-byte overlap).

Relocations are applied after all section data has been written, because the
addend is read from the output image.  Use `pread`/`pwrite` or `lseek` +
`read`/`write` with a 2-byte buffer.  Do not buffer the whole 64 KiB output.

### 5.4 Far/segment considerations

There are none in the current model.  The guest has one code segment and one
data segment covering the same 64 KiB.  `R_386_16` is a 16-bit offset, not a
selector.  `R_386_PC16` is a near relative displacement.  If a future
producer emits far pointers (segment:offset), the current relocation types
are insufficient; that is a new relocation/ABI design, not something the
linker can infer.  Do not emit segment relocations or a GDT/LDT from the
linker; `emu` owns the descriptors.

---

## 6. Archive (`.a`) handling

### 6.1 Is it required?

For the current `xcc` link, yes: it passes `-lc -lgcc`
(`xcc:31`) and `libgcc` is needed by `_printf.o` for `__udivsi3`/`__umodsi3`.
The host linker stage should therefore support archives so it can be a
drop-in replacement without changing the build.

For the final self-hosted build, archives are not fundamentally required.
Sections 6.5 and 8 give a staged plan: first link explicit objects, then add
archive support to the guest linker only if size permits.

### 6.2 `ar` format facts

Both `libc.a` and `libgcc.a` are GNU `ar` archives:

```
"!<arch>\n"
member header (60 bytes):
  name[16]  mtime[12]  uid[6]  gid[6]  mode[8]  size[10]  "`\n"
member data (size bytes, padded to even)
```

Special members:

* `/` — GNU archive symbol table (32-bit big-endian count, offsets, names).
  It can be skipped; a sequential member scan is simpler.
* `//` — GNU long-name string table.
* `#1/<len>` — BSD extended name (not used by the current archives).

Current archive names are short (`_printf.o`, `strlen.o`).  A robust parser
should strip the trailing `/` from normal names and support `/offset` names
via `//`; otherwise reject with a clear message.

GNU ar symbol-table offsets point at member **headers**; the member data
starts at `header_offset + 60`.  ELF section offsets inside a member are
relative to the member data base, so the linker's object parser needs a
`file_base` parameter.

### 6.3 Extraction algorithm

Process inputs in command-line order, matching GNU ld’s archive timing.  In
the current `xcc` link all regular objects precede the archives, but the
algorithm should still be defined for an interspersed command line:

1. Walk the command line.  Regular object files are included
   unconditionally.  Record their strong/weak definitions, commons, and
   referenced undefined names.  When an archive is reached, process it before
   moving on to later regular objects (so an archive never sees an undefined
   symbol that appears only in a later object, just like GNU ld).
2. For each archive, repeat a scan until no member is selected:
   * read each member header;
   * parse the member’s ELF symtab;
   * if any `STB_GLOBAL`/`STB_WEAK` **defined** symbol has a name currently
     in the referenced-undefined set, select that member;
   * append the member’s section data to the object list and add its
     relocation-referenced undefined global names to the set;
   * if the member defines a strong symbol already defined elsewhere,
     report a duplicate (unless overridden).
3. Later archives see the undefined set produced by earlier regular objects
   and archives.  This makes `-lc -lgcc` work: libc references
   `__udivsi3`/`__umodsi3`, then libgcc supplies them.
4. Members already selected are never pulled twice, even if an archive is
   named twice.

A per-archive fixed-point loop is needed because a pulled member can
introduce a new undefined symbol that another member of the *same* archive
defines (for example `__ia16_ldivmodu` is in the same libgcc archive as
`udivsi3.o`/`umodsi3.o`).

If the archive symbol table is parsed for speed, it is only an index; the
member’s own symtab remains authoritative.  The sequential algorithm above
is sufficient for 78/156 members and avoids implementing GNU hash tables.

### 6.4 Member/object descriptors

Record for each selected member:

```
uint32_t file_offset;   /* header offset or data offset */
uint32_t size;          /* member data size */
uint16_t archive_id;    /* which archive */
uint8_t  kind;          /* regular object / archive member */
```

With 128 regular objects + 192 members, a 16-byte descriptor is ~5 KiB.
That is within the guest budget but should be a configurable constant.

### 6.5 Plan to avoid archives in the self-hosted build

Several options, in order of increasing work:

1. **Explicit object lists.**  Link the linker itself and the compiler passes
   against explicit `.o` files.  The aggregate libc text is only ~6.2 KiB, so
   all 78 libc members can be passed explicitly without archives if the
   output still fits.  An `nm` survey of the current `libc.a` shows that its
   only non-libc undefined names are `__brk`, `__brk_end`, `__progname`,
   `_sys`, `__udivsi3` and `__umodsi3`; the first four are supplied by
   `crt0.o`/the script.  Add the three minimal libgcc members
   (`udivsi3.o`, `umodsi3.o`, `ia16-ldivmodu.o`) or implement
   `__udivsi3`/`__umodsi3` in the project runtime.
2. **One prelinked runtime object.**  At build time, use the host linker (or
   a future `-r`) to combine the subset of libc needed by the final programs
   into a single `libc-ld.o`, then let the guest linker consume only regular
   objects.  This is a bootstrap convenience; it should not become a
   permanent host dependency.
3. **A tiny project runtime.**  For the linker itself, implement only
   `open/read/write/lseek/close/brk` wrappers and the few string/memory
   helpers it needs, then link `ld.o` with those explicit objects.  This
   keeps the guest linker well under the segment limit and avoids pulling
   stdio/malloc.
4. **Archive support in the guest linker.**  Only after the ELF/relocation
   core works and if the size budget allows.  The sequential algorithm in
   §6.3 is written to be implementable with static tables and
   32-bit file offsets; it is not fundamentally too large.

Recommended bootstrap path: host linker supports archives; guest linker
starts with explicit objects and a tiny runtime; add archives to the guest
only if a later self-link needs them.

---

## 7. Command-line interface and internal architecture

### 7.1 CLI compatible with `xcc`

Proposed usage:

```
ld [options] file...
  -o FILE               output file (default a.out)
  -T SCRIPT             linker script (default built-in user.x equivalent)
  -mstack=N             define STACK_SIZE=N (default 2048)
  --defsym SYM=VAL      define an absolute symbol (repeatable)
  -L DIR                add a library search directory
  -l NAME               search for libNAME.a
  -l:FILE               use exactly FILE
  -O binary|elf         output format (default binary for emu)
  --entry SYM           entry symbol (default from ENTRY or _start)
  --map FILE            write a text map of sections/symbols
  -v                    verbose: archive members, section map, size
  -r                    partial link (future; not in first version)
  -s                    accepted and ignored for raw output
  -nostdlib             accepted and ignored
  -Wl,...               accepted and ignored for xcc compatibility
```

`-mstack=N` and `--defsym STACK_SIZE=N` are aliases.  Accept both
`--defsym SYM=VAL` and `--defsym=SYM=VAL` forms.  If `-T` is omitted, use a
built-in layout identical to `lib/user.x`.  For a transition period it is
also useful to recognise the current `xcc` spelling
`-Wl,--defsym,STACK_SIZE=N` and turn it into a `STACK_SIZE` definition; that
lets `xcc` swap only the linker path before the wrapper is cleaned up.

Current `xcc` passes `-Wl,--defsym,"STACK_SIZE=$nstack"` and then runs
`ia16-elf-objcopy` (`xcc:119-125`).  A direct integration should change only
the wrapper:

```sh
# before
ld=$cc
ldflags="-nostdlib -L$root/lib/libc -lc -lgcc -Wl,--no-warn-rwx-segments"
$ld -o "$elf" -Wl,--defsym,"STACK_SIZE=$nstack" -T "$ldscript" "$crt0" $objs $ldflags
$oc -O binary "$elf" "$_output"

# after (host-stage linker; later invoke through emu)
ld="$root/bin/ld/ld"
ldflags="-L$root/lib/libc -lc -L$gccdir -lgcc"
$ld -o "$_output" -mstack="$nstack" -T "$ldscript" "$crt0" $objs $ldflags
```

where `gccdir=$(dirname "$(ia16-elf-gcc -print-libgcc-file-name)")`.  The
current `-nostdlib` and `-Wl,--no-warn-rwx-segments` can be dropped, or the
linker can ignore them.  For the guest stage, `$ld` becomes
`$EMU $root/bin/ld/ld` (or a wrapper).  The `.elf` file, `ia16-elf-objcopy`,
and `ia16-elf-size` steps disappear; `-v` can print the size instead.

The linker must not rely on a compiler driver’s default library search path.
If `-lgcc` is used, `xcc` must supply `-L$gccdir` or an absolute archive path.

### 7.2 Files/modules

Suggested layout (names are illustrative):

| File | Responsibility |
| --- | --- |
| `ld.c` | argument parsing, driver, error output, map file |
| `elf.c` | ELF32 little-endian readers, header/section/symtab/rel validation |
| `ar.c` | `!<arch>` iteration, member headers, long names, selection scans |
| `script.c` | small script lexer/parser and expression evaluator |
| `layout.c` | output-section assignment, common allocation, dot/symbol evaluation |
| `reloc.c` | relocation loop, formulas, patching |
| `ld.h` | shared types, fixed limits, `read_at`/`write_at` |

The same source should build both host and guest.  Only the I/O layer differs
in name resolution; `open`, `read`, `write`, `lseek`, `close` exist in both
the host libc and the guest libc.  Use `long`/`off_t` file offsets and test
that the guest compiler’s `long` path (c0/c3 dword support) produces 32-bit
arithmetic; if it does not, wrap offsets in a two-word helper.

### 7.3 Static tables and no-malloc policy

No `malloc`, no `stdio`, no recursion over unbounded input.  Suggested first
limits (tune after measuring the compiler tree):

| Table | Limit | Approx. bytes |
| --- | --- | ---: |
| input objects/members | 128 | 2 KiB |
| alloc input sections | 512 × 2-byte address | 1 KiB |
| global symbols | 1024 × 8 | 8 KiB |
| name pool | 8192 | 8 KiB |
| referenced undefined names | 512 | (in name pool) |
| common symbols | 64 × 8 | 512 B |
| script symbols | 32 × 6 | 192 B |
| archives | 8 | small |
| archive members scanned | 192 × 8 | 1.5 KiB |
| I/O buffer | 512 | 512 B |

That is under ~22 KiB of static data; leave headroom for future features.
The linker’s own `.bss` plus stack (2 KiB) must fit under 64 KiB together with
its text.  If a limit is exceeded, report “too many …” rather than
overflowing an array.

Object descriptors should store file offsets and sizes as 32-bit values.
`libgcc.a` is 148382 bytes, so 16-bit member offsets are not enough; this is
also why the guest linker needs 32-bit `off_t`.

### 7.4 Two-pass layout/write flow

1. **Open/parse/select.**
   - Parse options, script, `--defsym`.
   - Open each input; validate ELF envelope.
   - Record sections by type/name; record global definitions, commons,
     undefined references from relocations.
   - Process archives in command order with the §6.3 fixed-point scan.
   - Add script/`--defsym` symbols.
2. **Layout.**
   - Walk output sections and matching input sections in script/object order.
   - Align and assign each input section a 16-bit final address; advance dot.
   - Allocate commons at `*(COMMON)`.
   - Evaluate dot assignments, `__brk`, `__brk_end`, `ASSERT`.
   - Set each global definition’s final value from its section address.
   - Validate entry == 0; validate `__brk <= __brk_end`, `file_end <= __brk`,
     `file_end <= 0x10000`.
3. **Write.**
   - Walk file-backed sections in address order.  Fill gaps with zeros from a
     small static zero buffer; copy section data in chunks.
   - For `SHT_NOBITS`, advance the address but write nothing.
   - Set the output file length to the highest file-backed end (this happens
     naturally by the last write; if the last section is NOBITS, nothing is
     written after the last PROGBITS, which is correct).
4. **Relocate.**
   - Re-open/seek each object; iterate every `SHT_REL` section.
   - Resolve and patch each entry with `pwrite`/`lseek`+`write`.
5. **Map/diagnostics.**
   - Optionally write a text map (section addresses, symbol names/values,
     archive member names, final sizes).
   - Print errors without stdio, using a small `write(2, ...)` formatter.

The output file can be opened once as `O_WRONLY|O_CREAT|O_TRUNC`; relocations
need read access, so use `O_RDWR` or reopen it.  `emu`’s libc has `pread` and
`pwrite` implemented via seek/read/write, so either approach works.  A
2-byte patch buffer is sufficient.

### 7.5 Why output is not buffered

A full 64 KiB output buffer would consume the entire guest data budget.
Streaming section data and patching 2 bytes at a time keeps static memory
small.  The linker only needs one 512-byte input buffer and one 512-byte zero
buffer.

### 7.6 Size budget

| Component | Target |
| --- | ---: |
| guest linker text | <= 28 KiB |
| guest linker static data | <= 24 KiB |
| linker stack | 2 KiB |
| linker binary on disk (text + initialised data) | <= 32 KiB |
| output programs | 1.8–8.5 KiB typical, < 64 KiB |

For comparison, the current guest binaries are `c0` 34500 bytes, `c3` 19998,
`c4` 8622, `as` 7986.  A 28–35 KiB linker is in the same range as `c0` and
should fit alongside a small runtime.  If the first guest version exceeds the
budget, drop the archive scanner, map file, and verbose diagnostics first;
then shrink the script parser.

---

## 8. Staged implementation plan, testing, sizes

### 8.1 Phase 0 — freeze the observations

* Keep this document and the survey script
  (`/tmp/opencode/elf_survey.py`) as the reference.
* Add test objects (not compiler changes) for: empty object, `.bss` only,
  section-symbol ABS16, section-symbol PC16, undefined global, common,
  duplicate, 1-byte symbol edge case.
* Record the GNU ld/objcopy reference outputs for `bin/*` and `lib/*` so
  parity can be checked later.

### 8.2 Phase 1 — host-built minimal linker

Deliverable: a host executable that links c4/as objects and the current
host objects to a raw image.

Scope:

* ELF32 parser, four output sections, section-symbol relocations, ABS16 and
  PC16, raw writer.
* Fixed built-in `user.x` layout; `-T` accepted and validated against the
  built-in script, or a minimal parser if time permits.
* No archives initially; test with `crt0.o`, `bin/cc/test.o`, and an explicit
  subset of libc objects (or a prelinked runtime object).
* `-mstack`, `--defsym`, `-o`.

Exit criteria:

* `emu` runs the linked `AAAAAAAA/BBBBBBBB` test.
* The c4/as-only object mix links without host ld.
* Error cases (undefined, duplicate, stack assertion, bad relocation,
  truncated object) behave cleanly.

### 8.3 Phase 2 — archives and `xcc` parity

Deliverable: a drop-in host linker for the current xcc link line.

Scope:

* `ar` parser, archive selection fixed point, common coalescing.
* `-L`/`-l`, `-lc`, `-lgcc`; add `gccdir` to xcc’s search path.
* Full minimal script parser; map output; verbose archive trace.
* Integrate behind an environment switch (`XCC_LD=bin/ld/ld` or a wrapper)
  while keeping GNU ld/objcopy as fallback.

Exit criteria:

* All `bin/*` programs and compiler passes build and run under `emu`.
* Link `crt0.o + bin/cc/test.o + libc.a + libgcc.a` and produce the expected
  output.
* Compare raw bytes with GNU ld + objcopy; where they differ, explain and
  verify behaviour (string merge, archive order, section padding).
* `-lgcc` resolves the three needed members; no undefined symbols remain.

### 8.4 Phase 3 — guest port and self-host

Deliverable: `bin/ld/ld` as a flat guest binary under `emu`.

Scope:

* Compile the same K&R C source with the project’s own `cpp/c0/c1/c2/c3/c4`.
* Keep all tables static, no `malloc`, no stdio; use guest libc syscalls only.
* Initially link the guest linker with explicit runtime objects rather than
  archives (§6.5).  If size allows, enable the archive scanner later.
* Keep the host linker as the bootstrap linker for `ld.o`.

Exit criteria:

* Guest linker links the `AAAAAAAA/BBBBBBBB` program and `bin/hello`.
* Guest linker links a c4/as object set.  If archive support is enabled at
  this stage, it also links an archive member set; otherwise it uses the
  explicit runtime objects from §6.5.
* **Self-link:** guest linker links its own `ld.o` + runtime, and the
  resulting `ld` produces a byte-identical (or verified-equivalent) program.
* Run under `emu` with `-mstack=2048`; verify no overlap with `.bss` and no
  stack corruption.
* Measure text/bss/binary sizes against §7.6.

### 8.5 Phase 4 — make it the default

* Change `xcc` to use `bin/ld` and drop `ia16-elf-ld`/`objcopy`/`size` from
  the normal path.
* Keep a documented `XCC_LD=gnu` fallback for one transition period.
* Decide whether to add `-r`, ELF-output mode, string merging, weak symbols,
  and archive support in the guest linker.  These are optional and must not
  compromise the 64 KiB budget.

### 8.6 Test matrix

| Test | What it checks |
| --- | --- |
| Parser unit objects | Header/section/symtab/rel validation, endian reads, bounds |
| Empty object | No sections, no symbols; layout still deterministic |
| `.bss`-only object | NOBITS contributes memory but no file bytes |
| Section-symbol ABS16 | Input-section address + addend, not output base |
| Section-symbol PC16 | `S + A − P`, addend −2, input-section base |
| Undefined global / archive | Pull order, fixpoint, final undefined error |
| Common + strong | COMMON coalescing, strong override, `.bss` placement |
| Duplicate strong | Diagnostic, no output |
| `db symbol` / 1-byte field | Reject or correctly diagnose the 2-byte relocation |
| Alignment | 16-byte text padding, 4-byte rodata, 2-byte data, bss |
| Raw extent | `.bss` not written, gaps zeroed, file size correct |
| `__brk`/`__brk_end` | Values, assert, stack too large/zero, heap overlap |
| Entry | `_start` at 0; nonzero entry rejected |
| GNU parity | `cmp`/section/symbol comparison plus `emu` output |
| Self-link | guest linker builds itself; second-generation output matches |
| Limits | Too many objects/symbols/relocations fails cleanly |
| Malformed input | Truncated ELF/ar, bad magic, unsupported reloc/hdr |

Run the functional tests under both the host and guest linker, and always run
the resulting image under `emu` (a clean link is not proof of correct code).

### 8.7 Expected sizes / reference numbers

| Artifact | Size |
| --- | ---: |
| `lib/crt0.o` | 672 bytes (text 32, bss 8) |
| c4 `bin/cc/test.o` | ~1.7 KiB (text 0x3e3, bss 0) |
| c4 `bin/as/as_self.o` | ~29 KiB object (text 0x66dd, bss 0x14ec) |
| `lib/libc/libc.a` | 59934 bytes, 78 members; aggregate text 6177, rodata 1175, data 8, bss 12, comment 1098 |
| `libgcc.a` | 148382 bytes, 156 members; aggregate text 42220; only 3 members needed |
| host hello raw | 8452 bytes; `__brk=0x2120`, `__brk_end=0xf800` |
| mixed own test raw | 1846 bytes; `__brk=0x744` |
| current guest pass binaries | c0 34500, c3 19998, c4 8622, as 7986 |

The guest linker should aim to be in the 28–35 KiB binary range.  If it is
larger than `c0`, re-check static table sizes and whether archive scanning
belongs in the guest version.

---

## 9. Open questions and risks

1. **Section-symbol semantics** — the single highest-risk area.  Every
   implementation test should include an object placed at a nonzero text
   offset and a relocation against a section symbol.  A naive “output base +
   addend” implementation passes single-object tests and fails real links.
2. **Local absolute fixups in c4/as** — baked into section bytes; `-r` and
   any reordering that changes an object’s section base are unsafe.  Current
   c3 output does not use absolute local labels, but `as` source can.  The
   long-term fix is producer-side: emit section-symbol relocations for local
   absolute references.
3. **Byte-for-byte GNU parity** — archive member choice, `.rodata.str*`
   merging, orphan section placement, and alignment can differ.  Define
   acceptance as “same addresses/behaviour for the script-controlled
   sections” and run binaries, not just `cmp`.  For c4/as-only objects,
   byte parity is a reasonable goal.
4. **Archive over-pull from stale objects** — old objects can contain
   undefined symbol entries with no relocation.  Drive extraction and final
   undefined checks from relocations.  If a producer needs every extern to be
   resolved, add an explicit flag rather than making it the default.
5. **libgcc search path** — a standalone linker has no GCC default library
   path.  `xcc` must pass `-L$(dirname $(ia16-elf-gcc -print-libgcc-file-name))`
   or an absolute path.  For the guest stage, avoid libgcc with explicit
   members or project-provided `__udivsi3`/`__umodsi3`.
6. **64 KiB and 32 KiB limits** — the linker must reject output > 65536;
   `emu` silently truncates.  PC16 can wrap across the segment, but debuggers
   and assemblers may be surprised by calls spanning more than 32 KiB.  Keep
   the compiler-pass text below 32 KiB or be prepared to explain wrap-around
   behaviour.
7. **Guest 32-bit arithmetic** — archive member offsets exceed 65535
   (`libgcc.a`), so the guest linker needs 32-bit file offsets.  Verify the
   project compiler’s `long`/dword path early; otherwise implement a
   two-word offset helper.
8. **Static table exhaustion** — the compiler tree is near several pool
   limits.  Choose generous symbol/section limits but fail cleanly; do not
   let a full table overwrite adjacent data.  Measure actual maxima by
   linking the tree.
9. **Entry-order coupling** — raw `emu` always starts at 0, so `crt0.o` must
   be first and `_start` must be at 0.  The linker should diagnose this
   explicitly rather than silently producing a binary that starts in the
   middle of `crt0`.
10. **`__brk` alignment and heap assumptions** — the current script leaves
    `__brk` wherever `.bss` ends.  If the linker adds trailing alignment,
    heap addresses change; that is acceptable if documented, but should not
    accidentally place `__brk` before file-backed data.
11. **`.rodata.str1.1` merging** — ignoring `SHF_MERGE` is semantically valid
    for C, but changes pointer equality and output size.  If exact GNU parity
    is required, implement content merging later; otherwise document.
12. **COMMON vs strong across mixed objects** — host libc has COMMON
    `errno`/`optind`/`optarg`/`optopt`; c4/as tentative definitions are
    strong `.bss` symbols.  The strong-wins rule must be implemented, or
    mixed host/own links will report duplicates or allocate twice.
13. **Script evolution** — the built-in subset covers `user.x`; if the
    script gains `ALIGN`, `PROVIDE`, `KEEP`, `SORT`, or output attributes,
    the linker should fail with a clear message rather than ignore them.
14. **Output ELF/map** — raw output loses symbol information.  A map file is
    recommended for debugging; a full ELF executable writer is optional and
    must not be on the critical path for `emu`.

---

## Appendix A — exact source references

| Behavior | Reference |
| --- | --- |
| c4 ELF header and 7 sections | `lib/c4/main.c:1107-1159`, `1204-1313` |
| c4 section/reloc/symtab writing | `lib/c4/main.c:392-416`, `1212-1266` |
| c4 symbol emission rule | `lib/c4/main.c:1162-1169` |
| c4 ABS16/PC16 construction | `lib/c4/main.c:418-492` |
| c4 local-label fixups | `lib/c4/main.c:250-271` |
| c4 section selection for data/strings | `lib/c4/main.c:1586-1632`, `1680-1692` |
| c4 static/global function flags | `lib/c4/main.c:1634-1678` |
| as ELF header and sections | `bin/as/as.c:1083-1135`, `1252-1289` |
| as reloc/symtab writing | `bin/as/as.c:398-423`, `1188-1242` |
| as section folding | `bin/as/as.c:337-342` |
| as local-symbol detection | `bin/as/as.c:273-290` |
| as ABS16/PC16 construction | `bin/as/as.c:425-499` |
| as local-label fixups | `bin/as/as.c:250-271` |
| xcc link/objcopy step | `xcc:16-31`, `119-125` |
| user script | `lib/user.x:1-27` |
| emu memory/segments | `emu/emu.c:148-167` |
| emu raw load | `emu/emu.c:277-313` |
| emu entry at IP 0 | `emu/emu.c:215-235` |
| emu argv/stack setup | `emu/emu.c:169-213` |
| crt0 entry | `lib/crt0.asm:1-35` |
| brk/sbrk | `lib/libc/unistd/brk.c:4-25` |
| malloc starts at `__brk` | `lib/libc/stdlib/malloc.c:20-52`, `108-114` |

## Appendix B — useful inspection commands

```sh
# What does an object contain?
ia16-elf-readelf -h -S -s -r bin/cc/test.o

# What does the current GNU link do?
ia16-elf-gcc -o /tmp/opencode/hello.elf -nostdlib \
  -Wl,--defsym,STACK_SIZE=2048 -T lib/user.x \
  lib/crt0.o bin/hello/hello.o \
  -Llib/libc -lc -L"$(dirname "$(ia16-elf-gcc -print-libgcc-file-name)")" -lgcc
ia16-elf-readelf -SW -s /tmp/opencode/hello.elf | grep -E '\.text|\.rodata|\.data|\.bss|__brk'
ia16-elf-objcopy -O binary /tmp/opencode/hello.elf /tmp/opencode/hello.bin
ls -l /tmp/opencode/hello.bin

# Archive survey used for this design
python3 /tmp/opencode/elf_survey.py lib/libc/libc.a \
  "$(ia16-elf-gcc -print-libgcc-file-name)"
```

The survey script lives outside the repository on purpose; it is an analysis
aid, not part of the build.
