#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

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
} config_t;

// Emulator states
typedef enum{
  QUIT,
  RUNNING,
  PAUSED,
} emulator_state_t;

// CHIP-8 Machine object
typedef struct{
  emulator_state_t state;
  uint8_t ram[4096];
  bool display[64*32];  // Emulate original CHIP-8 resolution pixels
  uint16_t stack[12];   // Subroutine stack
  uint8_t V[16];        // Data registers V0-VF
  uint16_t I;           // Index register
  uint16_t PC;          // Program Counter
  uint8_t delay_timer;  // Decrements at 60hz when >0 
  uint8_t sound_timer;  // Decrements at 60hz and plays tone when >0
  bool keypad[16];      // Hexadecimal keypad 0x0-0xF
  const char *rom_name; // Currently running ROM
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
    .bg_color = 0xFFFF00FF,  // YELLOW
    .scale_factor = 20,      // Default resolution will be 1280x640
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
void update_screen(const sdl_t sdl){
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

// The main function
int main(int argc,char **argv){
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

  // Main emulator loop
  while(chip8.state != QUIT){
    // Handle user input
    handle_input(&chip8);
    if (chip8.state == PAUSED) continue;

    // Get_time()
    // Emulate CHIP-8 Instructions
    // Get_time() elapsed since last get_time();

    // Delay for approximately 60hz/60fps (16.67ms)
    // SDL_Delay(16 - actual time elapsed);
    SDL_Delay(16);
    // Update window with changes
    update_screen(sdl);
  }

  // Final cleanup
  final_cleanup(sdl);

  exit(EXIT_SUCCESS);
}
