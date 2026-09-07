# emu-ia16

A user-mode emulator for 16-bit x86 (IA-16) programs on x86-64 Linux and
FreeBSD.  Instead of interpreting instructions, emu-ia16 runs guest code
natively on the CPU: the host program sets up a 16-bit protected mode code
and data segment pair in the LDT, loads a flat 64 KiB binary image into it,
and jumps into it.  A small 32-bit shim segment bridges guest 16-bit code
and the 64-bit host to dispatch system calls.

## How it works

1. `emu` mmaps a 64 KiB region below 4 GiB (`MAP_32BIT`) and installs two
   LDT descriptors covering it: a 16-bit code segment and a 16-bit data
   segment (the guest's entire address space).
2. A 32-bit shim (see `shim.S`) is copied to its own segment.  When the
   guest performs a far call to the shim, it switches to a 64-bit host
   stack, saves the guest's 16-bit registers, and calls `sysentry()` in
   `emu.c` on the host.
3. The guest binary is loaded at offset 0 of the segment.  Before entry,
   an initial stack in the shape of a normal process stack is built:
   `argc`, a NULL-terminated `argv[]`, and the argument strings packed
   at the top of the segment.
4. On entry, the guest's `crt0` (`lib/crt0.asm`) receives the far pointer
   to the syscall shim in `AX:CX`, stores it in `_sys`, and calls
   `main(argc, argv)`.

## Dependencies

- [bmk](https://github.com/realchonk/bmk) -- build tool
- `ia16-elf` toolchain (`ia16-elf-gcc`, `ia16-elf-ar`,
  `ia16-elf-objcopy`) -- cross compiler for 16-bit x86
- `nasm` -- assembler for the host shim, `crt0` and libc routines
- A C compiler and linker for the host emulator (defaults to `cc`)

## Building

```sh
mk [-o objdir]   # build emu, libc and all programs in bin/
mk clean         # remove build artefacts
```

## Usage

```sh
./emu prog [args...]
```

`prog` is a flat 16-bit binary image linked against the bundled
`crt0` and libc.  Programs can also be built and run in one go through
the `prog` template:

```sh
mk bin/hello/run
mk bin/echo/run ARGS="hello world"
```

## Writing programs

A program lives in its own directory under `bin/` with a `Mkfile` like
this:

```make
NAME = hello
OBJS = hello.o
.expand prog
```

The `prog` template (see `templates.mk`) links the objects with
`lib/crt0.o` against the bundled libc (`lib/libc`), using the linker
script `lib/user.x`, and converts the resulting ELF file to a flat
binary with `ia16-elf-objcopy`.

Programs are compiled with `-march=i286 -mcmodel=small
-ffreestanding`; the code generator assumes 16-bit 80286 protected mode
instructions.  `main()` is called with the usual `argc`/`argv`
arguments, with `argv` pointing into the guest data segment.

### Syscalls

System calls are issued by a far call to the entry point passed in
`AX:CX` at startup.  Currently implemented:

| Number | Call                      |
|--------|---------------------------|
| 0      | indirect: `syscall AX`    |
| 1      | `exit(int status)`        |
| 2      | `write(fd, buf, count)`   |

Any other syscall number prints a diagnostic to the host's stderr and
returns 0.
