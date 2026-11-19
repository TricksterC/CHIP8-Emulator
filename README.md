# CHIP-8 Emulator

A CHIP-8 emulator built from scratch in **C**, using **SDL2** for graphics and input handling.

This project is currently a work in progress. The goal is to implement the CHIP-8 virtual machine, understand its architecture and instruction set, and eventually make it playable directly in a web browser using **WebAssembly**.

## Features

- CHIP-8 interpreter implementation in C
- Instruction decoding and execution
- SDL2-based graphics and input handling
- ROM support

## Tech Stack

- C
- SDL2
- Make
- WebAssembly *(planned)*

## Build & Run

Clone the repository:

```bash
git clone https://github.com/TricksterC/CHIP8-Emulator
cd chip8-emulator
```

Build using Make:

```bash
make
```

Run the emulator:

```bash
./chip8 <ROM-file>
```
## ROMs

The `roms/` folder contains:

- **Test ROMs** (`roms/*.ch8`) used to verify opcode correctness
- **Games, demos, hires and programs** from the community-maintained
  [chip8-roms archive](https://github.com/kripod/chip8-roms)

All ROMs belong to their respective authors. The author and year are
included in most filenames, and the original archive README and
changelog are kept in `roms/`.

## ROMs

The `roms/` folder contains:

- **Test ROMs** (`roms/*.ch8`) used to verify opcode correctness
- **Games, demos, hires and programs** from the
  *Chip-8 Program Pack* by Revival Studios

The pack's original README is included unmodified as `roms/README.txt`.
All ROMs belong to their respective authors, and the author and year
are included in most filenames.

## Credits

- **Revival Studios**, for compiling the Chip-8 Program Pack (2011) and
  researching the original authors and release years of its ROMs
- The ROM authors, including David Winter, Joseph and Joyce Weisbecker,
  Carmelo Cortez, zeroZshadow, Sergey Naydenov, Fran Dachille, Paul Vervalin,
  Andreas Gustafsson, Matthew Mikolay, Hap, and many others. See the
  filenames and `.txt` files in `roms/` for the full list.

## Goals

- Complete CHIP-8 instruction set implementation
- Improve graphics, timing, and input handling
- Test compatibility with different CHIP-8 ROMs
- Compile to WebAssembly
- Host a playable browser version
