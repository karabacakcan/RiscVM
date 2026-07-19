# RiscVM

A small RISC-V (RV32I subset) instruction-set emulator written in C.

The emulator loads a flat binary image into memory, then fetches, decodes and
executes instructions until a halt is requested. It implements the core
integer instructions plus a set of memory-mapped I/O routines for console
input/output and a simple bank-based heap allocator (`malloc`/`free`).

## Layout

```
RiscVM.sln                      Visual Studio solution
RiscVM/
  RiscVM.vcxproj                Visual Studio project
  RiscVM.vcxproj.filters
  riscv_emulator.c              Emulator source
  examples/                     Sample programs (.c sources and .mi images)
```

## Supported instructions

- Register-register: `ADD`, `SUB`, `SLL`, `XOR`, `SRL`, `SRA`, `OR`, `AND`
- Register-immediate: `ADDI`, `SLTI`, `SLTIU`, `XORI`, `ORI`, `ANDI`
- Loads: `LB`, `LH`, `LW`, `LBU`
- Stores: `SB`, `SH`, `SW`
- Branches: `BEQ`, `BNE`, `BLT`, `BGE`, `BLTU`, `BGEU`
- Jumps: `JAL`, `JALR`
- Upper immediate: `LUI`

### Memory-mapped I/O

| Address  | Operation                     |
| -------- | ----------------------------- |
| `0x0800` | Console write character       |
| `0x0804` | Console write signed integer  |
| `0x0808` | Console write unsigned integer|
| `0x080C` | Halt                          |
| `0x0812` | Console read character        |
| `0x0816` | Console read signed integer   |
| `0x0820` | Dump program counter          |
| `0x0830` | Allocate heap memory (malloc) |
| `0x0834` | Free heap memory (free)       |

## Building

### Visual Studio

Open `RiscVM.sln` and build the `RiscVM` project (Debug|x64).

### Command line (GCC/Clang)

```sh
cd RiscVM
gcc -D_CRT_SECURE_NO_WARNINGS -o riscv_emulator riscv_emulator.c
```

## Running

The emulator currently loads `examples/hello_world/hello_world.mi` by default.
Run it from the `RiscVM` directory so the example path resolves:

```sh
cd RiscVM
./riscv_emulator
```
