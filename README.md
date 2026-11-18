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
git clone <repository-url>
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

## Goals

- Complete CHIP-8 instruction set implementation
- Improve graphics, timing, and input handling
- Test compatibility with different CHIP-8 ROMs
- Compile to WebAssembly
- Host a playable browser version
