#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

#include "SDL.h"
#include "SDL_events.h"
#include "SDL_keycode.h"
#include "SDL_log.h"
#include "SDL_render.h"

// SDL container object
typedef struct {
  SDL_Window *window;
  SDL_Renderer *renderer;
} sdl_t;

// Emulator configuration object
typedef struct{
  uint32_t window_width;      // SDL Window width 
  uint32_t window_height;     // SDL Window height
  uint32_t fg_color;          // Foreground color RGBA8888
  uint32_t bg_color;          // Background color RGBA8888
  uint32_t scale_factor;      // Amount to scale a CHIP-8 pixel by ex. 20x will be a 20x larger window
  bool pixels_outlines;       // Draw pixels outlines, yes or no
} config_t;

// Emulator states
typedef enum{
  QUIT,
  RUNNING,
  PAUSED,
} emulator_state_t;

// CHIP-8 Instruction format
typedef struct{
  uint16_t opcode;
  uint16_t NNN;     // 12 bit address/constant
  uint8_t NN;       // 8 bit constant
  uint8_t N;        // 4 bit constant
  uint8_t X;        // 4 bit register identifier
  uint8_t Y;        // 4 bit register identifier
}instruction_t;

// CHIP-8 Machine object
typedef struct{
  emulator_state_t state;
  uint8_t ram[4096];
  bool display[64*32];  // Emulate original CHIP-8 resolution pixels
  uint16_t stack[12];   // Subroutine stack
  uint16_t *stack_ptr;  // 
  uint8_t V[16];        // Data registers V0-VF
  uint16_t I;           // Index register
  uint16_t PC;          // Program Counter
  uint8_t delay_timer;  // Decrements at 60hz when >0 
  uint8_t sound_timer;  // Decrements at 60hz and plays tone when >0
  bool keypad[16];      // Hexadecimal keypad 0x0-0xF
  const char *rom_name; // Currently running ROM
  instruction_t inst;   // Currently executing instruction
} chip8_t;

// Initialize SDL
bool init_sdl(sdl_t *sdl, const config_t config){
  if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0){
    SDL_Log("Could not initialize SDL subsystems! %s\n", SDL_GetError());
    return false;
  }
  
  sdl -> window = SDL_CreateWindow("CHIP-8 Emulator", SDL_WINDOWPOS_CENTERED, 
                                  SDL_WINDOWPOS_CENTERED, 
                                  config.window_width * config.scale_factor, 
                                  config.window_height * config.scale_factor, 
                                  0);

  if(!sdl -> window){
    SDL_Log("Could not create SDL window %s\n", SDL_GetError());
    return false;
  }

  sdl -> renderer = SDL_CreateRenderer(sdl -> window, -1, SDL_RENDERER_ACCELERATED);
  if(!sdl -> renderer){
    SDL_Log("Could not create SDL renderer %s\n", SDL_GetError());
    return false;
  }

  return true; // Success 
}

// Set up intial emulator configuration from passed in arguments
bool set_config_from_args(config_t *config, const int argc, char **argv){
  // Set defaults 
  *config = (config_t){
    .window_width = 64,      // CHIP-8 original X resolution
    .window_height = 32,     // CHIP-8 original Y resolution
    .fg_color = 0xFFFFFFFF,  // WHITE
    .bg_color = 0x000000FF,  // BLACK
    .scale_factor = 20,      // Default resolution will be 1280x640
    .pixels_outlines = true, // Draw pixels outlines by default
  };

  // Override defaults from passed in arguments
  for (int i = 1; i < argc; i++){
    (void)argv[i]; // Prevent compiler error from unused variables argc/argv
  // ...
  }
  return true; // Success 
}

// Initialize CHIP-8 Machine
bool init_chip8(chip8_t *chip8, const char *rom_name){
  const uint32_t entry_point = 0x200; //CHIP-8 ROMs will be laoded to 0x200
  const uint8_t font[] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
    0x20, 0x60, 0x20, 0x20, 0x70, // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
    0x90, 0x90, 0xF0, 0x10, 0x10, // 4
    0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
    0xF0, 0x10, 0x20, 0x40, 0x40, // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
    0xF0, 0x90, 0xF0, 0x90, 0x90, // A
    0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
    0xF0, 0x80, 0x80, 0x80, 0xF0, // C
    0xE0, 0x90, 0x90, 0x90, 0xE0, // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
    0xF0, 0x80, 0xF0, 0x80, 0x80,  // F
  };

  // Load font
  memcpy(&chip8 -> ram[0], font, sizeof(font));

  // Open ROM file
  FILE *rom = fopen(rom_name, "rb");
  if(!rom){
    SDL_Log("ROM file %s is invalid or does not exist\n", rom_name);
    return false;
  }

  // Get/check ROM size
  fseek(rom, 0, SEEK_END);
  const size_t rom_size = ftell(rom);
  const size_t max_size = sizeof chip8 -> ram - entry_point;
  rewind(rom);

  if(rom_size > max_size){
    SDL_Log("ROM file %s is too big! ROM size: %zu, Max size allowed: %zu\n", rom_name, rom_size, max_size);
    return false;
  }
  
  // Load ROM
  if(fread(&chip8 -> ram[entry_point], rom_size, 1, rom) != 1){
    SDL_Log("Could not read ROM file %s into ROM memory", rom_name);
    return false;
  }

  fclose(rom);

  // Set CHIP-8 machine defaults
  chip8 -> state = RUNNING;     // Default Machine state to on/running 
  chip8 -> PC = entry_point;    // Start program counter at ROM entry point 
  chip8 -> rom_name = rom_name; 
  chip8 -> stack_ptr = &chip8 -> stack[0];

  return true;  // Success
}

// Final Cleanup
void final_cleanup(const sdl_t sdl){
  SDL_DestroyRenderer(sdl.renderer);
  SDL_DestroyWindow(sdl.window);
  SDL_Quit(); // Shut down SDL subsytems
}

void clear_screen(const sdl_t sdl, const config_t config){
  const uint8_t r = (config.bg_color >> 24) & 0xFF;
  const uint8_t g = (config.bg_color >> 16) & 0xFF;
  const uint8_t b = (config.bg_color >>  8) & 0xFF;
  const uint8_t a = (config.bg_color >>  0) & 0xFF;

  SDL_SetRenderDrawColor(sdl.renderer, r, g, b, a);
  SDL_RenderClear(sdl.renderer);
}

// Update window with any changes
void update_screen(const sdl_t sdl, const config_t config, const chip8_t chip8){
  SDL_Rect rect = {.x = 0, .y = 0, .w = config.scale_factor, .h = config.scale_factor};
  
  // Grab colors to draw
  const uint8_t fg_r = (config.fg_color >> 24) & 0xFF;
  const uint8_t fg_g = (config.fg_color >> 16) & 0xFF;
  const uint8_t fg_b = (config.fg_color >>  8) & 0xFF;
  const uint8_t fg_a = (config.fg_color >>  0) & 0xFF;

  const uint8_t bg_r = (config.bg_color >> 24) & 0xFF;
  const uint8_t bg_g = (config.bg_color >> 16) & 0xFF;
  const uint8_t bg_b = (config.bg_color >>  8) & 0xFF;
  const uint8_t bg_a = (config.bg_color >>  0) & 0xFF;
  
  for(uint32_t i = 0; i < sizeof chip8.display; i++){
    // Translate 1D index i value to 2D X/Y coordinates
    // X = i % window_width 
    // Y = i / window_width 
    rect.x = (i % config.window_width) * config.scale_factor;
    rect.y = (i / config.window_width) * config.scale_factor;


    if(chip8.display[i]){
      // Pixel is on, draw foreground color
      SDL_SetRenderDrawColor(sdl.renderer, fg_r, fg_g, fg_b, fg_a);
      SDL_RenderFillRect(sdl.renderer, &rect);

      // If user requested drawing pixel outlines, draw those here
      if(config.pixels_outlines){
        SDL_SetRenderDrawColor(sdl.renderer, bg_r, bg_g, bg_b, bg_a);
        SDL_RenderDrawRect(sdl.renderer, &rect);
      }
      
    }
    else{
      // Pixel is off, draw background color
      SDL_SetRenderDrawColor(sdl.renderer, bg_r, bg_g, bg_b, bg_a);
      SDL_RenderFillRect(sdl.renderer, &rect);
    }
  }

  SDL_RenderPresent(sdl.renderer);
}

// Handle user input
void handle_input(chip8_t *chip8){
  SDL_Event event;
  while(SDL_PollEvent(&event)){
    switch(event.type){
      case SDL_QUIT:
        // Exit window; end program
        chip8 -> state = QUIT;
        return;

      case SDL_KEYDOWN:
        switch(event.key.keysym.sym){
          case SDLK_ESCAPE:
            // Escape Key; Exit window & end program
            chip8 -> state = QUIT;
            return;

          case SDLK_SPACE:
            // Space bar
            if(chip8 -> state == RUNNING){
              chip8 -> state = PAUSED;
              puts("====== PAUSED ======");  // PAUSED
            } 
            else
              chip8 -> state = RUNNING;      // RESUMED

          default:
            break;
        }
        break;

      case SDL_KEYUP:
        break;

      default:
        break;
    }

  }
}

#ifdef DEBUG
void print_debug_info(chip8_t *chip8){
  printf("Address: 0x%04X, Opcode: 0x%04X Desc: ", 
      chip8 -> PC -2, 
      chip8 -> inst.opcode);

  switch ((chip8 -> inst.opcode >> 12 )  & 0x0F){

    case 0x00:
      if(chip8 -> inst.NN == 0xE0){
        // 0x00E0: Clear the Screen
        printf("Clear screen\n");
      }
      else if(chip8 -> inst.NN == 0xEE){
        // 0x00EE: Return from Subroutine
        // Set program counter to last address on subroutine stack ("pop" it off the stack)
        // so that the next opcode will be gotten from that address.
        printf("Return from subroutine to address 0x%04X\n", 
            *(chip8 -> stack_ptr - 1));
        //chip8 -> PC = *--chip8 -> stack_ptr;
      } 
      else{
        printf("Unimplemented opcode\n");
      }
     break;

    case 0x01:
      // Ox1NNN: Jump to address NNN
      //chip8 -> PC = chip8 -> inst.NNN; // Set program counter so that next opcode is from NNN
      printf("Jump to address NNN (0x%04X)\n", 
          chip8 -> inst.NNN);
      break;

    case 0x02:
      // 0x2NNN: Call Subroutine at NNN
      // Store current address to return to on subroutine stack ("push" it on the stack)
      // and set program counter to subroutine address so that the next opcode 
      // is gotten from there.
      //*chip8 -> stack_ptr++ = chip8 -> PC;  
      //chip8 -> PC = chip8 -> inst.NNN;
      printf("Call subroutine at NNN (0x%04X), return address 0x%04X\n",
         chip8 -> inst.NNN, chip8 -> PC);
      break;

    case 0x03:
      // 0x03XNN: Check if VX == NN, if yes skip the next instruction
      //if(chip8 -> V[chip8 -> inst.X] != chip8 -> inst.NN)
       // chip8 -> PC += 2; // Skip next opcode or instruction
      printf("Check if V%X (0x%02X) == NN (0x%02X), skip next instruction if true\n", 
          chip8 -> inst.X, 
          chip8 -> V[chip8 -> inst.X], 
          chip8 -> inst.NN);
      break;

    case 0x04:
      // 0x4XNN: Check if VX != NN, if yes skip the next instruction
      //if(chip8 -> V[chip8 -> inst.X] != chip8 -> inst.NN)
      // chip8 -> PC += 2; // Skip next opcode or instruction
      printf("Check if V%X (0x%02X) != NN (0x%02X), skip next instruction if true\n", 
        chip8 -> inst.X, 
        chip8 -> V[chip8 -> inst.X], 
        chip8 -> inst.NN);
      break;

    case 0x05:
      // 0x5XY0: Check if VX == VY, if yes skip the next instruction
      // if(chip8 -> inst.NN != 0) break;
      
      // if(chip8 -> V[chip8 -> inst.X] != chip8 -> V[chip8 -> inst.Y])
       // chip8 -> PC += 2; // Skip next opcode or instruction
      printf("Check if V%X (0x%02X) == V%X (0x%02X), skip next instruction if true\n", 
        chip8 -> inst.X, chip8 -> V[chip8 -> inst.X], 
        chip8 -> inst.Y, chip8 -> V[chip8 -> inst.Y]);
      break;


    case 0x06:
      // 0x6XNN: Set register VX to NN
      //chip8 -> V[chip8 -> inst.X] = chip8 -> inst.NN;
      printf("Set V%X = NN (0x%02X)\n",
          chip8 -> inst.X, 
          chip8 -> inst.NN);
      break;

    case 0x07:
      // 0x7XNN: Set register VX += NN
      //chip8 -> V[chip8 -> inst.X] += chip8 -> inst.NN;
      printf("Set V%X (0x%02X) += NN (0x%02X). Result: 0x%02X\n",
          chip8 -> inst.X, 
          chip8 -> V[chip8 -> inst.X], 
          chip8 -> inst.NN, 
          chip8 -> V[chip8 -> inst.X] + chip8 -> inst.NN);
      break;

    case 0x0A:
      // 0xANNN: Set Index register I to NNN
      //chip8 -> I = chip8 -> inst.NNN;        
      printf("Set I to NNN (0x%04X)\n", 
          chip8 -> inst.NNN);
        break;

    case 0x0B:
      // 0xBNNN: Jump to V0 + NNN
      // chip8 -> PC = chip8 -> V[0] + chip8 -> inst.NNN;
      printf("Set PC to V[0] (0x%02X) + NNN (0x%02X); Result PC = 0x%04X\n",
          chip8 -> V[0], 
          chip8 -> inst.NNN, 
          chip8 -> V[0] + chip8 -> inst.NNN);
      break;  

    case 0x0C:
       // 0xCNNN: Sets register VX = rand() % 256 & NN (Bitwise AND)
       printf("Set V%X = rand() %% 256 & NN (0x%02X)\n",
           chip8 -> inst.X,
           chip8 -> inst.NN);
       break;



    case 0x0D:
       // 0xDXYN: Draw N-height sprite at coordinates X and Y; Read from memory location I
      // Screen pixels are XOR'd with sprite bits
      // VF(Carry flag) is set if any screen pixels are set off; This is useful for collision 
      // detection or other reasons.
      printf("Draw N(%u) height sprite at coordinates V%X (0x%02X), V%X (0x%02X)"
          " from memory location I (0x%04X). Set VF = 1 if any pixels are turned off.\n",
          chip8 -> inst.N, 
          chip8 -> inst.X, chip8 -> V[chip8 -> inst.X], 
          chip8 -> inst.Y, chip8 -> V[chip8 -> inst.Y], 
          chip8 -> I);
      break;

    case 0x08:
      switch(chip8 -> inst.N){
        case 0:
          // 0x8XY0: Set register VX = VY
          // chip8 -> V[chip8 -> inst.X] = chip8 -> V[chip8 -> inst.Y];
          printf("Set V%X = V%X (0x%02X)\n",
              chip8 -> inst.X, 
              chip8 -> inst.Y, 
              chip8 -> V[chip8 -> inst.Y]);
          break;
        
        case 1: 
          // 0x8XY1: Set register VX |= VY
          // chip8 -> V[chip8 -> inst.X] |= chip8 -> V[chip8 -> inst.Y];
          printf("Set V%X (0x%02X) |= V%X (0x%02X); Result: 0x%02X\n",
              chip8 -> inst.X, chip8 -> V[chip8 -> inst.X],
              chip8 -> inst.Y, chip8 -> V[chip8 -> inst.Y], 
              chip8 -> V[chip8 -> inst.X] | chip8 -> V[chip8 -> inst.Y]);
          break;

        case 2: 
          // 0x8XY2: Set register VX &= VY
          // chip8 -> V[chip8 -> inst.X] &= chip8 -> V[chip8 -> inst.Y];
          printf("Set V%X (0x%02X) &= V%X (0x%02X); Result: 0x%02X\n",
              chip8 -> inst.X, chip8 -> V[chip8 -> inst.X],
              chip8 -> inst.Y, chip8 -> V[chip8 -> inst.Y], 
              chip8 -> V[chip8 -> inst.X] & chip8 -> V[chip8 -> inst.Y]);
          break;

        case 3:
          // 0x8XY3: Set register VX ^= VY
          //chip8 -> V[chip8 -> inst.X] ^= chip8 -> V[chip8 -> inst.Y];
          printf("Set V%X (0x%02X) ^= V%X (0x%02X); Result: 0x%02X\n",
              chip8 -> inst.X, chip8 -> V[chip8 -> inst.X],
              chip8 -> inst.Y, chip8 -> V[chip8 -> inst.Y], 
              chip8 -> V[chip8 -> inst.X] ^ chip8 -> V[chip8 -> inst.Y]);
          break;

        case 4: 
          // 0x8XY4: Set register VX += VY, Set VF to 1 if carry
          //if((uint16_t)chip8 -> V[chip8 -> inst.X] + chip8 -> V[chip8 -> inst.Y] > 255)
            // chip8 -> V[0xF] = 1;
          // chip8 -> V[chip8 -> inst.X] += chip8 -> V[chip8 -> inst.Y];
          printf("Set V%X (0x%02X) += V%X (0x%02X), VF = 1 if carry; Result: 0x%02X, VF = %X\n",
              chip8 -> inst.X, chip8 -> V[chip8 -> inst.X],
              chip8 -> inst.Y, chip8 -> V[chip8 -> inst.Y], 
              chip8 -> V[chip8 -> inst.X] + chip8 -> V[chip8 -> inst.Y],
              (uint16_t)(chip8 -> V[chip8 -> inst.X] + chip8 -> V[chip8 -> inst.Y] > 255));
          break;

        case 5: 
          // 0x8XY5: Set register VX -= VY, Set VF to 1 if there is not a borrow (result is positive/0)
          // if(chip8 -> V[chip8 -> inst.Y] <= chip8 -> V[chip8 -> inst.X])
            // chip8 -> V[0xF] = 1;
          //chip8 -> V[chip8 -> inst.X] -= chip8 -> V[chip8 -> inst.Y];
          printf("Set V%X (0x%02X) -= V%X (0x%02X), VF = 1 if borrow; Result: 0x%02X, VF = %X\n",
              chip8 -> inst.X, chip8 -> V[chip8 -> inst.X],
              chip8 -> inst.Y, chip8 -> V[chip8 -> inst.Y], 
              chip8 -> V[chip8 -> inst.X] - chip8 -> V[chip8 -> inst.Y],
              (chip8 -> V[chip8 -> inst.Y] <= chip8 -> V[chip8 -> inst.X]));
          break;

        case 6: 
          // 0x8XY6: Set register VX >>= 1, store shifted off bit in VF 
          //chip8 -> V[0xF] = chip8 -> V[chip8 -> inst.X] & 1;
          //chip8 -> V[chip8 -> inst.X] >>= 1;
          printf("Set V%X (0x%02X) >>= 1, VF = shifted off bit (%X); Result: 0x%02X\n",
              chip8 -> inst.X, chip8 -> V[chip8 -> inst.X],
              chip8 -> V[chip8 -> inst.X] & 1, 
              chip8 -> V[chip8 -> inst.X] >> 1);
          break;

        case 7:
          // Ox8XY7: Set register VX = VY - VX, Set VF to 1 if there is not a borrow (result is positive/0)
          // if(chip8 -> V[chip8 -> inst.X] <= chip8 -> V[chip8 -> inst.Y])
            // chip8 -> V[0xF] = 1;
          // chip8 -> V[chip8 -> inst.X] = chip8 -> V[chip8 -> inst.Y] - chip8 -> V[chip8 -> inst.X];
          printf("Set V%X = V%X (0x%02X) - V%X (0x%02X), VF = 1 if borrow; Result: 0x%02X, VF = %X\n",
              chip8 -> inst.X, 
              chip8 -> inst.Y, chip8 -> V[chip8 -> inst.Y],
              chip8 -> inst.X, chip8 -> V[chip8 -> inst.X], 
              chip8 -> V[chip8 -> inst.Y] - chip8 -> V[chip8 -> inst.X],
              (chip8 -> V[chip8 -> inst.X] <= chip8 -> V[chip8 -> inst.Y])); 
          break;

        case 0xE:
          // 0x8XYE: Store register VX <<= 1, store shifter off but in VF 
          // chip8 -> V[0xF] = (chip8 -> V[chip8 -> inst.X] & 0x80) >> 7;
          // chip8 -> V[chip8 -> inst.X] <<= 1;
          printf("Set V%X (0x%02X) <<= 1, VF = shifted off bit (%X); Result: 0x%02X\n",
              chip8 -> inst.X, chip8 -> V[chip8 -> inst.X],
              (chip8 -> V[chip8 -> inst.X] & 0x80) >> 7, 
              chip8 -> V[chip8 -> inst.X] >> 1);
          break;

        default:
          // Wrong or Unimplemented
          break;

      }
      break;

    case 0x09:
      // 0x9XY0: Check if VX != VY; Skip next instruction if so
      // if (chip8 -> V[chip8 -> inst.X] != chip8 -> V[chip8 -> inst.Y])
        // chip8 -> PC += 2;
      printf("Check if V%X (0x%02X) != V%X (0x%02X), skip next instruction if true\n", 
        chip8 -> inst.X, chip8 -> V[chip8 -> inst.X], 
        chip8 -> inst.Y, chip8 -> V[chip8 -> inst.Y]);
      break;


    default:
      printf("Unimplemented opcode\n");
      break; // Unimplemented or invalid opcode
  }
}
#endif

void emulate_instruction(chip8_t *chip8, const config_t config){
  // Get next opcode from RAM 
  chip8 -> inst.opcode = (chip8 -> ram[chip8 -> PC] << 8) | chip8 -> ram[chip8 -> PC + 1];
  chip8 -> PC += 2; // Pre-increment program counter for next opcode
  
  // Fill out current instruction format
  chip8 -> inst.NNN = chip8 -> inst.opcode & 0x0FFF;
  chip8 -> inst.NN = chip8 -> inst.opcode & 0x0FF;
  chip8 -> inst.N = chip8 -> inst.opcode & 0x0F;
  chip8 -> inst.X = (chip8 -> inst.opcode >> 8) & 0x0F;
  chip8 -> inst.Y = (chip8 -> inst.opcode >> 4) & 0x0F;

#ifdef DEBUG
  print_debug_info(chip8);
#endif

  // Emulate opcode
  switch ((chip8 -> inst.opcode >> 12 )  & 0x0F){

    case 0x00:
      if(chip8 -> inst.NN == 0xE0){
        // 0x00E0: Clear the Screen
        memset(chip8 -> display, false, sizeof(chip8 -> display));
      }
      else if(chip8 -> inst.NN == 0xEE){
        // 0x00EE: Return from Subroutine
        // Set program counter to last address on subroutine stack ("pop" it off the stack)
        // so that the next opcode will be gotten from that address.
        chip8 -> PC = *--chip8 -> stack_ptr;
      }
      else{
        // Unimplemented/invalid opcode: maybe 0xNNN for calling machine code routine for RCA1802
      }
      break;

    case 0x01:
      // Ox1NNN: Jump to address NNN
      chip8 -> PC = chip8 -> inst.NNN; // Set program counter so that next opcode is from NNN
      break;

    case 0x02:
      // 0x2NNN: Call Subroutine at NNN
      // Store current address to return to on subroutine stack ("push" it on the stack)
      // and set program counter to subroutine address so that the next opcode 
      // is gotten from there.
      *chip8 -> stack_ptr++ = chip8 -> PC;  
      chip8 -> PC = chip8 -> inst.NNN;
      break;

    case 0x03:
      // 0x3XNN: Check if VX == NN, if yes skip the next instruction
      if(chip8 -> V[chip8 -> inst.X] == chip8 -> inst.NN)
        chip8 -> PC += 2; // Skip next opcode or instruction
      break;

    case 0x04:
      // 0x4XNN: Check if VX != NN, if yes skip the next instruction
      if(chip8 -> V[chip8 -> inst.X] != chip8 -> inst.NN)
        chip8 -> PC += 2; // Skip next opcode or instruction
      break;

    case 0x05:
      // 0x5XY0: Check if VX == VY, if yes skip the next instruction
      if(chip8 -> inst.N != 0) break;
      
      if(chip8 -> V[chip8 -> inst.X] == chip8 -> V[chip8 -> inst.Y])
        chip8 -> PC += 2; // Skip next opcode or instruction
      break;

    case 0x06:
      // 0x6XNN: Set register VX to NN
      chip8 -> V[chip8 -> inst.X] = chip8 -> inst.NN;
      break;

    case 0x07:
      // 0x7XNN: Set register VX += NN
      chip8 -> V[chip8 -> inst.X] += chip8 -> inst.NN;
      break;

    case 0x08:
      switch(chip8 -> inst.N){
        case 0:
          // 0x8XY0: Set register VX = VY
          chip8 -> V[chip8 -> inst.X] = chip8 -> V[chip8 -> inst.Y];
          break;
        
        case 1: 
          // 0x8XY1: Set register VX |= VY
          chip8 -> V[chip8 -> inst.X] |= chip8 -> V[chip8 -> inst.Y];
          break;

        case 2: 
          // 0x8XY2: Set register VX &= VY
          chip8 -> V[chip8 -> inst.X] &= chip8 -> V[chip8 -> inst.Y];
          break;

        case 3:
          // 0x8XY3: Set register VX ^= VY
          chip8 -> V[chip8 -> inst.X] ^= chip8 -> V[chip8 -> inst.Y];
          break;

        case 4:{
          // 0x8XY4: Set register VX += VY, Set VF to 1 if carry
          const bool carry = (uint16_t)(chip8 -> V[chip8 -> inst.X] + chip8 -> V[chip8 -> inst.Y] > 255);
          chip8 -> V[chip8 -> inst.X] += chip8 -> V[chip8 -> inst.Y];
          chip8 -> V[0xF] = carry;
          break;
        }

        case 5:{ 
          // 0x8XY5: Set register VX -= VY, Set VF to 1 if there is not a borrow (result is positive/0)
          const bool no_borrow1 = (chip8 -> V[chip8 -> inst.Y] <= chip8 -> V[chip8 -> inst.X]);
          chip8 -> V[chip8 -> inst.X] -= chip8 -> V[chip8 -> inst.Y];
          chip8 -> V[0xF] = no_borrow1;
          break;
        }

        case 6:{
          // 0x8XY6: Set register VX >>= 1, store shifted off bit in VF 
          const bool bit1 =  chip8 -> V[chip8 -> inst.X] & 1;
          chip8 -> V[chip8 -> inst.X] >>= 1;
          chip8 -> V[0xF] = bit1;
          break;
        }

        case 7:{
          // Ox8XY7: Set register VX = VY - VX, Set VF to 1 if there is not a borrow (result is positive/0)
          const bool no_borrow2 = chip8 -> V[chip8 -> inst.X] <= chip8 -> V[chip8 -> inst.Y];
          chip8 -> V[chip8 -> inst.X] = chip8 -> V[chip8 -> inst.Y] - chip8 -> V[chip8 -> inst.X];
          chip8 -> V[0xF] = no_borrow2;
          break;
        }

        case 0xE:{
          // 0x8XYE: Store register VX <<= 1, store shifter off but in VF 
          const bool bit2 = (chip8 -> V[chip8 -> inst.X] & 0x80) >> 7;
          chip8 -> V[chip8 -> inst.X] <<= 1;
          chip8 -> V[0xF] = bit2;
          break;
        }

        default:
          // Wrong or Unimplemented
          break;

      }
      break;

    case 0x09:
    // 0x9XY0: Check if VX != VY; Skip next instruction if so
    if (chip8 -> V[chip8 -> inst.X] != chip8 -> V[chip8 -> inst.Y])
      chip8 -> PC += 2;
    break;

    case 0x0A:
      // 0xANNN: Set Index register I to NNN
      chip8 -> I = chip8 -> inst.NNN;
      break;

    case 0x0B:
      // 0xBNNN: Jump to V0 + NNN
      chip8 -> PC = chip8 -> V[0] + chip8 -> inst.NNN;
      break;

    case 0x0C:
      // 0xCNNN: Sets register VX = rand() % 256 & NN (Bitwise AND)
      chip8 -> V[chip8 -> inst.X] = (rand() % 256) & chip8 -> inst.NN;
      break;

    case 0x0D:
      // 0xDXYN: Draw N-height sprite at coordinates X and Y; Read from memory location I
      // Screen pixels are XOR'd with sprite bits
      // VF(Carry flag) is set if any screen pixels are set off; This is useful for collision 
      // detection or other reasons.

      chip8 -> V[0xF] = 0; // Initialize carry flag to 0

      uint8_t X_coordinate = chip8 -> V[chip8 -> inst.X] % config.window_width;
      uint8_t Y_coordinate = chip8 -> V[chip8 -> inst.Y] % config.window_height;
      const uint8_t original_X_coordinate = X_coordinate; // Original X value

      // Loop over all N rows of the sprite
      for(uint8_t i = 0; i < chip8 -> inst.N; i++){
        const uint8_t sprite_data = chip8 -> ram[chip8 -> I + i];
        X_coordinate = original_X_coordinate; // Reset X for next row to draw

        for(int8_t j = 7; j >= 0; j--){
          // If sprite pixel/bit is on and display pixel is on, set carry flag
          bool *pixel = &chip8 -> display[Y_coordinate * config.window_width + X_coordinate];
          const bool sprite_bit = (sprite_data & (1 << j));

          if(sprite_bit && *pixel){
            chip8 -> V[0xF] = 1;
          }
          // XOR display pixel with sprite pixel/bit
          *pixel ^= sprite_bit; 

          // Stop drawing this row if hit right edge of screen
          if(++X_coordinate >= config.window_width) break;
        }

        // Stop drawing entire sprite if hit bottom edge of screen
        if(++Y_coordinate >= config.window_height) break;
      }
    break;


    default:
      break; // Unimplemented or invalid opcode
  }

}

// The main function
int main(int argc,char **argv){
  // Default usage message for args 
  if(argc < 2){
    fprintf(stderr, "Usage: %s <rom_name>\n",argv[0]);
    exit(EXIT_FAILURE);
  }

  //  Initialize Emulator configuration/options
  config_t config = {0};
  if (!set_config_from_args(&config, argc, argv)) exit(EXIT_FAILURE);


  // Initialize SDL
  sdl_t sdl = {0};
  if (!init_sdl(&sdl, config)) exit(EXIT_FAILURE);

  // Initialize CHIP-8 Machine
  chip8_t chip8 = {0};
  const char *rom_name = argv[1];
  if(!init_chip8(&chip8, rom_name)) exit(EXIT_FAILURE);

  // Initial Screen clear to Background color
  clear_screen(sdl, config);

  // Seed Random number Generator
  srand(time(NULL));

  // Main emulator loop
  while(chip8.state != QUIT){
    // Handle user input
    handle_input(&chip8);
    if (chip8.state == PAUSED) continue;

    // Get_time()

    // Emulate CHIP-8 Instructions
    emulate_instruction(&chip8, config);

    // Get_time() elapsed since last get_time();

    // Delay for approximately 60hz/60fps (16.67ms)
    // SDL_Delay(16 - actual time elapsed);
    SDL_Delay(16);
    // Update window with changes
    update_screen(sdl, config, chip8);
  }

  // Final cleanup
  final_cleanup(sdl);

  exit(EXIT_SUCCESS);
}
