/*
  Things the bot code needs to be able to do.
  Get the current image of the screen.
  Send keyboard / joystick input.
  Draw debug things to the screen.

  ProcessFrame will be called inline in sdl because it will be able to write to the screen.
  Need to start screen capture from the platform layer. Passing screens to ProcessFrame.
 */
#ifndef _aissac_platform_h
#define _aissac_platform_h

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include <math.h>

// Set up some type aliases I like.

typedef int32_t  i32;
typedef int64_t  i64;
typedef uint8_t  u8;
typedef uint32_t u32;
typedef uint64_t u64;
typedef float    r32;
typedef double   r64;

// Common macros
#define LEN(e) (sizeof(e) / sizeof(e[0]))
#define INFO(format, ...) fprintf(stderr, "[INFO]  (%s:%d) " format "\n", \
                                      __FILE__, __LINE__, ##__VA_ARGS__)
#define ERROR(format, ...) fprintf(stderr, "[ERROR] (%s:%d) " format "\n", \
                                      __FILE__, __LINE__, ##__VA_ARGS__)
#define ASSERT(c) assert(c)

// Types for communication between platform code and AI/Bot code.
typedef struct {

} GameInput;

typedef struct {

} Memory;

// Interface between platform and AI/Bot.
typedef GameInput (*ProcessFrame)(Memory *memory);


#endif
