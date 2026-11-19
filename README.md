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

## Credits

Thanks to the people who created the ROMs included here, including:

- David Winter (Blitz, Connect 4, Merlin, Space Invaders, Maze, and more)
- Joseph and Joyce Weisbecker (Kaleidoscope, Rocket, Slide, Sum Fun, and more)
- Carmelo Cortez (Breakout, Craps, Nim, Submarine, and more)
- Revival Studios (Astro Dodge, Trip8 Demo, Worm V4)
- zeroZshadow (Particle Demo, Zero Demo, ZeroPong)
- Sergey Naydenov (Sierpinski, Stars, SQRT Test)
- Fran Dachille (Tetris), Paul Vervalin (Pong), Andreas Gustafsson (Brix),
  Matthew Mikolay (timer and RNG tests), Hap (Rush Hour, Fishie,
  Keypad Test), and many others

See `roms/README.txt` and the filenames for the full list of authors.

## Goals

- Complete CHIP-8 instruction set implementation
- Improve graphics, timing, and input handling
- Test compatibility with different CHIP-8 ROMs
- Compile to WebAssembly
- Host a playable browser version
